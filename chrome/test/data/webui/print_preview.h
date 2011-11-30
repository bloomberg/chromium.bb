// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_PRINT_PREVIEW_H_
#define CHROME_TEST_DATA_WEBUI_PRINT_PREVIEW_H_
#pragma once

#include "base/command_line.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"

class PrintPreviewWebUITest : public WebUIBrowserTest {
 public:
  PrintPreviewWebUITest();
  virtual ~PrintPreviewWebUITest();

 private:
  // WebUIBrowserTest implementation.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewWebUITest);
};

#endif  // CHROME_TEST_DATA_WEBUI_PRINT_PREVIEW_H_
