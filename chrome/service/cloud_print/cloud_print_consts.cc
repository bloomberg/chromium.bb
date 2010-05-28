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

// Values in the respone JSON from the cloud print server
const wchar_t kPrinterListValue[] = L"printers";
const wchar_t kSuccessValue[] = L"success";
const wchar_t kNameValue[] = L"name";
const wchar_t kIdValue[] = L"id";
const wchar_t kTicketUrlValue[] = L"ticketUrl";
const wchar_t kFileUrlValue[] = L"fileUrl";
const wchar_t kJobListValue[] = L"jobs";
const wchar_t kTitleValue[] = L"title";
const wchar_t kPrinterCapsHashValue[] = L"capsHash";

const char kDefaultCloudPrintServerUrl[] = "https://www.google.com/cloudprint";
const char kCloudPrintTalkServiceUrl[] = "http://www.google.com/cloudprint";
const char kGaiaUrl[] = "https://www.google.com/accounts/ClientLogin";
const char kCloudPrintGaiaServiceId[] = "cloudprint";
const char kSyncGaiaServiceId[] = "chromiumsync";

