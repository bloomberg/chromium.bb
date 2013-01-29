// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/print_preview.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

PrintPreviewWebUITest::PrintPreviewWebUITest() {}

PrintPreviewWebUITest::~PrintPreviewWebUITest() {}

// Always print preview for testing.
void PrintPreviewWebUITest::SetUpCommandLine(CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
#if !defined(GOOGLE_CHROME_BUILD)
  command_line->AppendSwitch(switches::kEnablePrintPreview);
#endif
}
