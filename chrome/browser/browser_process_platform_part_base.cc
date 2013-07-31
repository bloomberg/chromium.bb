// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/browser_process_platform_part_base.h"
#include "chrome/browser/lifetime/application_lifetime.h"

BrowserProcessPlatformPartBase::BrowserProcessPlatformPartBase() {
}

BrowserProcessPlatformPartBase::~BrowserProcessPlatformPartBase() {
}

void BrowserProcessPlatformPartBase::PlatformSpecificCommandLineProcessing(
    const CommandLine& /* command_line */) {
}

void BrowserProcessPlatformPartBase::StartTearDown() {
}

void BrowserProcessPlatformPartBase::AttemptExit() {
// chrome::CloseAllBrowsers() doesn't link on OS_IOS and OS_ANDROID, but
// OS_ANDROID overrides this method already and OS_IOS never calls this.
#if defined(OS_IOS) || defined(OS_ANDROID)
  NOTREACHED();
#else
  // On most platforms, closing all windows causes the application to exit.
  chrome::CloseAllBrowsers();
#endif
}

void BrowserProcessPlatformPartBase::PreMainMessageLoopRun() {
}
