// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_SHELL_SWITCHES_H_
#define HEADLESS_APP_HEADLESS_SHELL_SWITCHES_H_

#include "content/public/common/content_switches.h"

namespace headless {
namespace switches {
extern const char kCrashDumpsDir[];
extern const char kDeterministicFetch[];
extern const char kDumpDom[];
extern const char kHideScrollbars[];
extern const char kProxyServer[];
extern const char kRemoteDebuggingAddress[];
extern const char kRepl[];
extern const char kScreenshot[];
extern const char kTimeout[];
extern const char kUseGL[];
extern const char kUserDataDir[];
extern const char kVirtualTimeBudget[];
extern const char kWindowSize[];

// Switches which are replicated from content.
using ::switches::kHostResolverRules;
using ::switches::kRemoteDebuggingPort;

}  // namespace switches
}  // namespace headless

#endif  // HEADLESS_APP_HEADLESS_SHELL_SWITCHES_H_
