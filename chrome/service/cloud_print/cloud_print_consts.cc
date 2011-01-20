// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constant defines used in the cloud print proxy code

#include "chrome/service/cloud_print/cloud_print_consts.h"

const char kProxyIdValue[] = "proxy";
const char kPrinterNameValue[] = "printer";
const char kPrinterDescValue[] = "description";
const char kPrinterCapsValue[] = "capabilities";
const char kPrinterDefaultsValue[] = "defaults";
const char kPrinterStatusValue[] = "status";
const char kPrinterTagValue[] = "tag";
const char kPrinterRemoveTagValue[] = "remove_tag";
const char kMessageTextValue[] = "message";

// Values in the respone JSON from the cloud print server
const char kPrinterListValue[] = "printers";
const char kSuccessValue[] = "success";
const char kNameValue[] = "name";
const char kIdValue[] = "id";
const char kTicketUrlValue[] = "ticketUrl";
const char kFileUrlValue[] = "fileUrl";
const char kJobListValue[] = "jobs";
const char kTitleValue[] = "title";
const char kPrinterCapsHashValue[] = "capsHash";
const char kTagsValue[] = "tags";
const char kProxyTagPrefix[] = "__cp__";
const char kTagsHashTagName[] = "__cp__tagshash";
const char kTagDryRunFlag[] = "__cp__dry_run";


const char kDefaultCloudPrintServerUrl[] = "https://www.google.com/cloudprint";
const char kGaiaUrl[] = "https://www.google.com/accounts/ClientLogin";
const char kCloudPrintGaiaServiceId[] = "cloudprint";
const char kSyncGaiaServiceId[] = "chromiumsync";
const char kCloudPrintPushNotificationsSource[] = "cloudprint.google.com";
// The cloud print server expects the X-Google-CloudPrint-Proxy header for
// certain requests.
const char kChromeCloudPrintProxyHeader[] = "X-CloudPrint-Proxy: Chrome";

// The string to be appended to the user-agent for cloudprint requests.
const char kCloudPrintUserAgent[] = "GoogleCloudPrintProxy";

// Reasons for fetching print jobs.
// Job fetch on proxy startup.
const char kJobFetchReasonStartup[] = "startup";
// Job fetch because we are polling.
const char kJobFetchReasonPoll[] = "poll";
// Job fetch on being notified by the server.
const char kJobFetchReasonNotified[] = "notified";
// Job fetch after a successful print to query for more jobs.
const char kJobFetchReasonQueryMore[] = "querymore";

// Short message ids for diagnostic messages sent to the server.
const char kPrintSystemFailedMessageId[] = "printsystemfail";
const char kGetPrinterCapsFailedMessageId[] = "getprncapsfail";

