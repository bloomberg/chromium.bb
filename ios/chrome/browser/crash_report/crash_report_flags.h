// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_FLAGS_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_FLAGS_H_

#include "base/feature_list.h"

namespace crash_report {

extern const base::Feature kBreakpadNoDelayInitialUpload;

// Feature to enable the detection of freeze in the main thread
extern const base::Feature kDetectMainThreadFreeze;

// The different timeout value for kDetectMainThreadFreeze.
extern const char kDetectMainThreadFreezeParameterName[];
extern const char kDetectMainThreadFreezeParameter3s[];
extern const char kDetectMainThreadFreezeParameter5s[];
extern const char kDetectMainThreadFreezeParameter7s[];
extern const char kDetectMainThreadFreezeParameter9s[];

// Returns a delay after which a crash report is generated if the main thread is
// frozen. Returns 0 if the feature is disabled.
int TimeoutForMainThreadFreezeDetection();

}  // namespace crash_report

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORT_FLAGS_H_
