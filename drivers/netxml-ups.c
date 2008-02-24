/* netxml-ups.c	Driver routines for network XML UPS units 

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ne_request.h>
#include <ne_basic.h>
#include <ne_props.h>
#include <ne_uri.h>
#include <ne_xmlreq.h>

#include "main.h"

#include "netxml-ups.h"
#include "mge-xml.h"

#define	DRV_VERSION	"0.10"

/* Global vars */
uint32_t		ups_status = 0;
static subdriver_t	*subdriver = &mge_xml_subdriver;
static ne_session	*session;

/* Support functions */
static void ups_alarm_set(void);
static void ups_status_set(void);

void upsdrv_initinfo(void)
{
	int		ret;
	ne_request	*request;
	ne_xml_parser	*parser;

	upsdebugx(3, "ne_request_create(session, \"GET\", \"%s\");", subdriver->initinfo);

	if (strlen(subdriver->initinfo) < 1) {
		fatalx(EXIT_FAILURE, "%s: failure to read initinfo element", __func__);
	}

	request = ne_request_create(session, "GET", subdriver->initinfo);

	/* Create an XML parser. */
	parser = ne_xml_create();

	/* Push a new handler on the parser stack */
	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

	ret = ne_xml_dispatch_request(request, parser);

	if (ret != NE_OK) {
		upslogx(LOG_ERR, "Failed: %s", ne_get_error(session));
	}

	ne_xml_destroy(parser);
	ne_request_destroy(request);

	dstate_setinfo("driver.version.internal", "%s", subdriver->version);

	/* dstate_setinfo("ups.mfr", "skel driver"); */
	/* dstate_setinfo("ups.model", "longrun 15000"); */

	/* upsh.instcmd = instcmd; */
}

void upsdrv_updateinfo(void)
{
	int		ret;
	ne_request	*request;
	ne_xml_parser	*parser;

	upsdebugx(3, "ne_request_create(session, \"GET\", \"%s\");", subdriver->updateinfo);

	if (strlen(subdriver->updateinfo) < 1) {
		fatalx(EXIT_FAILURE, "%s: failure to read updateinfo element", __func__);
	}

	request = ne_request_create(session, "GET", subdriver->updateinfo);

	/* Create an XML parser. */
	parser = ne_xml_create();

	/* Push a new handler on the parser stack */
	ne_xml_push_handler(parser, subdriver->startelm_cb, subdriver->cdata_cb, subdriver->endelm_cb, NULL);

	ret = ne_xml_dispatch_request(request, parser);

	if (ret != NE_OK) {
		upslogx(LOG_ERR, "Failed: %s", ne_get_error(session));
		dstate_datastale();
	}

	ne_xml_destroy(parser);
	ne_request_destroy(request);

	status_init();

	alarm_init();
	ups_alarm_set();
	alarm_commit();

	ups_status_set();
	status_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

/*
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}
*/

/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.test.interval")) {
		ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* allow '-x xyzzy' */
	/* addvar(VAR_FLAG, "xyzzy", "Enable xyzzy mode"); */

	/* allow '-x foo=<some value>' */
	/* addvar(VAR_VALUE, "foo", "Override foo setting"); */
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - network XML UPS driver %s (%s)\n\n", 
		DRV_VERSION, UPS_VERSION);
	experimental_driver = 1;
}

void upsdrv_initups(void)
{
	ne_uri		uri;

	/* Initialize socket libraries */
	if (ne_sock_init()) {
		fatalx(EXIT_FAILURE, "%s: failed to initialize socket libraries", progname);
	}

	/* Parse the URI argument. */
	if (ne_uri_parse(device_path, &uri) || uri.host == NULL) {
		fatalx(EXIT_FAILURE, "%s: invalid hostname '%s'\n", progname, device_path);
	}

	if (uri.scheme == NULL) {
		uri.scheme = "http";
	}

	if (uri.port == 0) {
		uri.port = ne_uri_defaultport(uri.scheme);
	}

	upsdebugx(1, "using %s://%s port %d", uri.scheme, uri.host, uri.port);

	/* create the session */
	session = ne_session_create(uri.scheme, uri.host, uri.port);

	/* Sets the user-agent string */
	ne_set_useragent(session, subdriver->version);
#if 0
	/* Load default CAs if using SSL. */
	if (strcasecmp(uri.scheme, "https") == 0)
		if (ne_ssl_load_default_ca(session))
			fprintf(stdout, "Failed to load default CAs.\n");
#endif
}

void upsdrv_cleanup(void)
{
	ne_session_destroy(session);
}

/**********************************************************************
 * Support functions
 *********************************************************************/

/* Convert the local status information to NUT format and set NUT
   alarms. */
static void ups_alarm_set(void)
{
	if (ups_status & STATUS_BIT(REPLACEBATT)) {
		alarm_set("Replace battery!");
	}
	if (ups_status & STATUS_BIT(SHUTDOWNIMM)) {
		alarm_set("Shutdown imminent!");
	}
	if (ups_status & STATUS_BIT(FANFAIL)) {
		alarm_set("Fan failure!");
	}
	if (ups_status & STATUS_BIT(NOBATTERY)) {
		alarm_set("No battery installed!");
	}
	if (ups_status & STATUS_BIT(BATTVOLTLO)) {
		alarm_set("Battery voltage too low!");
	}
	if (ups_status & STATUS_BIT(BATTVOLTHI)) {
		alarm_set("Battery voltage too high!");
	}
	if (ups_status & STATUS_BIT(CHARGERFAIL)) {
		alarm_set("Battery charger fail!");
	}
	if (ups_status & STATUS_BIT(OVERHEAT)) {
		alarm_set("Temperature too high!");	/* overheat; Belkin, TrippLite */
	}
	if (ups_status & STATUS_BIT(COMMFAULT)) {
		alarm_set("Internal UPS fault!");	/* UPS fault; Belkin, TrippLite */
	}
	if (ups_status & STATUS_BIT(AWAITINGPOWER)) {
		alarm_set("Awaiting power!");		/* awaiting power; Belkin, TrippLite */
	}
}

/* Convert the local status information to NUT format and set NUT
   status. */
static void ups_status_set(void)
{
	if (ups_status & STATUS_BIT(VRANGE)) {
		dstate_setinfo("input.transfer.reason", "input voltage out of range");
	} else if (ups_status & STATUS_BIT(FRANGE)) {
		dstate_setinfo("input.transfer.reason", "input frequency out of range");
	} else {
		dstate_delinfo("input.transfer.reason");
	}

	if (ups_status & STATUS_BIT(ONLINE)) {
		status_set("OL");		/* on line */
	} else {
		status_set("OB");               /* on battery */
	}
	if ((ups_status & STATUS_BIT(DISCHRG)) &&
		!(ups_status & STATUS_BIT(DEPLETED))) {
		status_set("DISCHRG");	        /* discharging */
	}
	if ((ups_status & STATUS_BIT(CHRG)) &&
		!(ups_status & STATUS_BIT(FULLYCHARGED))) {
		status_set("CHRG");		/* charging */
	}
	if (ups_status & (STATUS_BIT(LOWBATT) | STATUS_BIT(TIMELIMITEXP) | STATUS_BIT(SHUTDOWNIMM))) {
		status_set("LB");		/* low battery */
	}
	if (ups_status & STATUS_BIT(OVERLOAD)) {
		status_set("OVER");		/* overload */
	}
	if (ups_status & STATUS_BIT(REPLACEBATT)) {
		status_set("RB");		/* replace batt */
	}
	if (ups_status & STATUS_BIT(TRIM)) {
		status_set("TRIM");		/* SmartTrim */
	}
	if (ups_status & STATUS_BIT(BOOST)) {
		status_set("BOOST");	        /* SmartBoost */
	}
	if (ups_status & STATUS_BIT(BYPASS)) {
		status_set("BYPASS");	        /* on bypass */   
	}
	if (ups_status & STATUS_BIT(OFF)) {
		status_set("OFF");              /* ups is off */
	}
	if (ups_status & STATUS_BIT(CAL)) {
		status_set("CAL");		/* calibration */
	}
}
