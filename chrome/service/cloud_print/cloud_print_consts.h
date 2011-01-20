// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONSTS_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONSTS_H_
#pragma once

#include "base/basictypes.h"

// Constant defines used in the cloud print proxy code
extern const char kProxyIdValue[];
extern const char kPrinterNameValue[];
extern const char kPrinterDescValue[];
extern const char kPrinterCapsValue[];
extern const char kPrinterDefaultsValue[];
extern const char kPrinterStatusValue[];
extern const char kPrinterTagValue[];
extern const char kPrinterRemoveTagValue[];
extern const char kMessageTextValue[];
// Values in the respone JSON from the cloud print server
extern const char kPrinterListValue[];
extern const char kSuccessValue[];
extern const char kNameValue[];
extern const char kIdValue[];
extern const char kTicketUrlValue[];
extern const char kFileUrlValue[];
extern const char kJobListValue[];
extern const char kTitleValue[];
extern const char kPrinterCapsHashValue[];
extern const char kTagsValue[];
extern const char kProxyTagPrefix[];
extern const char kTagsHashTagName[];
extern const char kTagDryRunFlag[];
extern const char kDefaultCloudPrintServerUrl[];
extern const char kGaiaUrl[];
extern const char kCloudPrintGaiaServiceId[];
extern const char kSyncGaiaServiceId[];
extern const char kCloudPrintPushNotificationsSource[];
extern const char kChromeCloudPrintProxyHeader[];
extern const char kCloudPrintUserAgent[];
extern const char kJobFetchReasonStartup[];
extern const char kJobFetchReasonPoll[];
extern const char kJobFetchReasonNotified[];
extern const char kJobFetchReasonQueryMore[];
extern const char kPrintSystemFailedMessageId[];
extern const char kGetPrinterCapsFailedMessageId[];

// Max retry count for job data fetch requests.
const int kJobDataMaxRetryCount = 5;
// Max retry count (infinity) for API fetch requests.
const int kCloudPrintAPIMaxRetryCount = -1;

// When we don't have XMPP notifications available, we resort to polling for
// print jobs. We choose a random interval in seconds between these 2 values.
const int kMinJobPollIntervalSecs = 5*60;  // 5 minutes in seconds
const int kMaxJobPollIntervalSecs = 8*60;  // 8 minutes in seconds

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONSTS_H_

