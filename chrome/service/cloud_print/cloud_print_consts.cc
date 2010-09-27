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
const char kPrinterTagsValue[] = "tags";
const char kProxyTagPrefix[] = "__cp__";
const char kTagsHashTagName[] = "__cp__tagshash";
const char kLocationTagName[] = "location";
const char kDriverNameTagName[] = "drivername";


const char kDefaultCloudPrintServerUrl[] = "https://www.google.com/cloudprint";
const char kGaiaUrl[] = "https://www.google.com/accounts/ClientLogin";
const char kCloudPrintGaiaServiceId[] = "cloudprint";
const char kSyncGaiaServiceId[] = "chromiumsync";
const char kCloudPrintPushNotificationsSource[] = "cloudprint.google.com";

