// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace cloud_print {

const char kCloudPrintUserAgent[] = "GoogleCloudPrintProxy";
const char kChromeCloudPrintProxyHeader[] = "X-CloudPrint-Proxy: Chrome";
const char kCloudPrintGaiaServiceId[] = "cloudprint";
const char kProxyAuthUserAgent[] = "ChromiumBrowser";
const char kCloudPrintPushNotificationsSource[] = "cloudprint.google.com";

const char kProxyIdValue[] = "proxy";
const char kPrinterNameValue[] = "printer";
const char kPrinterDescValue[] = "description";
const char kPrinterCapsValue[] = "capabilities";
const char kPrinterDefaultsValue[] = "defaults";
const char kPrinterStatusValue[] = "status";
const char kPrinterTagValue[] = "tag";
const char kPrinterRemoveTagValue[] = "remove_tag";
const char kMessageTextValue[] = "message";

const char kPrintSystemFailedMessageId[] = "printsystemfail";
const char kGetPrinterCapsFailedMessageId[] = "getprncapsfail";
const char kEnumPrintersFailedMessageId[] = "enumfail";
const char kZombiePrinterMessageId[] = "zombieprinter";

const char kSuccessValue[] = "success";
const char kNameValue[] = "name";
const char kIdValue[] = "id";
const char kTicketUrlValue[] = "ticketUrl";
const char kFileUrlValue[] = "fileUrl";
const char kPrinterListValue[] = "printers";
const char kJobListValue[] = "jobs";
const char kTitleValue[] = "title";
const char kPrinterCapsHashValue[] = "capsHash";
const char kTagsValue[] = "tags";
const char kXMPPJidValue[] = "xmpp_jid";
const char kOAuthCodeValue[] = "authorization_code";
const char kCreateTimeValue[] = "createTime";
const char kPrinterTypeValue[] = "type";

const char kChromeVersionTagName[] = "chrome_version";
const char kSystemNameTagName[] = "system_name";
const char kSystemVersionTagName[] = "system_version";

const char kCloudPrintServiceProxyTagPrefix[] = "__cp__";
const char kCloudPrintServiceTagsHashTagName[] = "__cp__tagshash";
const char kCloudPrintServiceTagDryRunFlag[] = "__cp__dry_run";

const char kJobFetchReasonStartup[] = "startup";
const char kJobFetchReasonPoll[] = "poll";
const char kJobFetchReasonNotified[] = "notified";
const char kJobFetchReasonQueryMore[] = "querymore";
const char kJobFetchReasonFailure[] = "failure";
const char kJobFetchReasonRetry[] = "retry";

}  // namespace cloud_print

