// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/single_language_options_browsertest.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

SingleLanguageOptionsBrowserTest::SingleLanguageOptionsBrowserTest() {}

SingleLanguageOptionsBrowserTest::~SingleLanguageOptionsBrowserTest() {}

void SingleLanguageOptionsBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kDisableMultilingualSpellChecker);
}
