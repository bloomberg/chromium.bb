// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_constants.h"
namespace content {

const base::FilePath::CharType kAppCacheDirname[] =
    FILE_PATH_LITERAL("Application Cache");
const base::FilePath::CharType kPepperDataDirname[] =
    FILE_PATH_LITERAL("Pepper Data");

const char kBrowserPluginMimeType[] = "application/browser-plugin";

// This number used to be limited to 32 in the past (see b/535234).
const size_t kMaxRendererProcessCount = 82;
const int kMaxSessionHistoryEntries = 50;
const size_t kMaxTitleChars = 4 * 1024;
const size_t kMaxURLChars = 2 * 1024 * 1024;
const size_t kMaxURLDisplayChars = 32 * 1024;

#if defined(GOOGLE_CHROME_BUILD)
const char kStatsFilename[] = "ChromeStats2";
#else
const char kStatsFilename[] = "ChromiumStats2";
#endif

const int kStatsMaxThreads = 32;
const int kStatsMaxCounters = 3000;

const int kHistogramSynchronizerReservedSequenceNumber = 0;

const char kGpuCompositingFieldTrialName[] = "ForceCompositingMode";
const char kGpuCompositingFieldTrialForceCompositingEnabledName[] = "enabled";
const char kGpuCompositingFieldTrialThreadEnabledName[] = "thread";

}  // namespace content
