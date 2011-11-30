// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/print_preview.h"

PrintPreviewWebUITest::PrintPreviewWebUITest() {}

PrintPreviewWebUITest::~PrintPreviewWebUITest() {}

// Always print preview for testing.
void PrintPreviewWebUITest::SetUpCommandLine(CommandLine* command_line) {
  WebUIBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kEnablePrintPreview);
}
