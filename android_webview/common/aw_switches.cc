// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_switches.h"

#include "base/command_line.h"

namespace switches {

const char kEnableUbercomp[] = "enable-ubercomp";

bool UbercompEnabled() {
  static bool ubercomp_enabled =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableUbercomp);
  return ubercomp_enabled;
}

}  // namespace switches
