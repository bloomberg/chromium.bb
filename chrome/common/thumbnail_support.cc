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

#if defined(OS_WIN)
  // Disables in-browser thumbnailing on Windows XP where not supported yet.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
  return true;
#elif defined(OS_LINUX) && !defined(USE_AURA)
  // Disables in-browser thumbnailing on non-Aura Linux where not supported yet.
  return false;
#else
  return true;
#endif
}
