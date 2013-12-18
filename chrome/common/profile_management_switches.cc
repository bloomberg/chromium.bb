// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profile_management_switches.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace switches {

bool IsEnableInlineSignin() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableInlineSignin);
}

bool IsGoogleProfileInfo() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kGoogleProfileInfo);
}

bool IsNewProfileManagement() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNewProfileManagement);
}

}  // namespace switches
