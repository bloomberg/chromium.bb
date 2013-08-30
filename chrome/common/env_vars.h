// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the environment variables used by Chrome.

#ifndef CHROME_COMMON_ENV_VARS_H__
#define CHROME_COMMON_ENV_VARS_H__

namespace env_vars {

extern const char kHeadless[];
extern const char kLogFileName[];
extern const char kMetroConnected[];
extern const char kSessionLogDir[];
extern const char kShowRestart[];
extern const char kRestartInfo[];
extern const char kRtlLocale[];
extern const char kLtrLocale[];
extern const char kStartupTestsNumCycles[];

// Google Update named environment variable that implies kSystemLevel.
// TODO(erikwright): Put this in chrome/installer/util/util_constants.h when
// http://crbug.com/174953 is fixed and widely deployed.
extern const char kGoogleUpdateIsMachineEnvVar[];

}  // namespace env_vars

#endif  // CHROME_COMMON_ENV_VARS_H__
