// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_ui_supervised_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/test_utils.h"

void DownloadsWebUIForSupervisedUsersTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
}
