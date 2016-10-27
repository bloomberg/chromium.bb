// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/titlebar_config.h"

#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"

bool ShouldCustomDrawSystemTitlebar() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kWindows10CustomTitlebar) &&
         base::win::GetVersion() >= base::win::VERSION_WIN10;
}
