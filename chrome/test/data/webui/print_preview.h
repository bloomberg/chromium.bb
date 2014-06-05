// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_PRINT_PREVIEW_H_
#define CHROME_TEST_DATA_WEBUI_PRINT_PREVIEW_H_

#include "chrome/test/base/web_ui_browser_test.h"

class PrintPreviewWebUITest : public WebUIBrowserTest {
 public:
  PrintPreviewWebUITest();
  virtual ~PrintPreviewWebUITest();

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintPreviewWebUITest);
};

#endif  // CHROME_TEST_DATA_WEBUI_PRINT_PREVIEW_H_
