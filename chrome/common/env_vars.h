// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the environment variables used by Chrome.

#ifndef CHROME_COMMON_ENV_VARS_H__
#define CHROME_COMMON_ENV_VARS_H__
#pragma once

namespace env_vars {

extern const char kHeadless[];
extern const char kLogFileName[];
extern const char kSessionLogDir[];
extern const char kEtwLogging[];
extern const char kShowRestart[];
extern const char kRestartInfo[];
extern const char kRtlLocale[];
extern const char kLtrLocale[];
extern const char kNoOOBreakpad[];
extern const char kStartupTestsNumCycles[];

}  // namespace env_vars

#endif  // CHROME_COMMON_ENV_VARS_H__
