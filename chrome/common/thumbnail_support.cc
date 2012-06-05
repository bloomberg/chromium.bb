// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/thumbnail_support.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

bool ShouldEnableInBrowserThumbnailing() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInBrowserThumbnailing))
    return false;

#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}
