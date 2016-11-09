// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/titlebar_config.h"

#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"
#include "ui/native_theme/native_theme_win.h"

bool ShouldCustomDrawSystemTitlebar() {
  // There's no reason to custom-draw the titlebar when high-contrast mode is on
  // because we would want to do exactly what Windows is doing anyway.
  return !ui::NativeThemeWin::instance()->IsUsingHighContrastTheme() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kWindows10CustomTitlebar) &&
         base::win::GetVersion() >= base::win::VERSION_WIN10;
}
