// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/chrome/athena_chrome_browser_test.h"

#include "athena/test/base/test_util.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/content_switches.h"

namespace athena {

AthenaChromeBrowserTest::AthenaChromeBrowserTest() {
}

AthenaChromeBrowserTest::~AthenaChromeBrowserTest() {
}

void AthenaChromeBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // The NaCl sandbox won't work in our browser tests.
  command_line->AppendSwitch(switches::kNoSandbox);
  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void AthenaChromeBrowserTest::SetUpOnMainThread() {
  // Set the memory pressure to low and turning off undeterministic resource
  // observer events.
  test_util::SendTestMemoryPressureEvent(ResourceManager::MEMORY_PRESSURE_LOW);
}

content::BrowserContext* AthenaChromeBrowserTest::GetBrowserContext() {
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace athena
