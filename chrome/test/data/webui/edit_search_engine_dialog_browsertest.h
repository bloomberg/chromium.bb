// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_DATA_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_BROWSERTEST_H_
#pragma once

#include "chrome/browser/ui/webui/web_ui_browsertest.h"

// Test framework for the WebUI based edit search engine dialog.
class EditSearchEngineDialogUITest : public WebUIBrowserTest {
 protected:
  EditSearchEngineDialogUITest();
  virtual ~EditSearchEngineDialogUITest();

  // Displays the search engine dialog for testing.
  void ShowSearchEngineDialog();

 private:
  DISALLOW_COPY_AND_ASSIGN(EditSearchEngineDialogUITest);
};

#endif  // CHROME_TEST_DATA_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_BROWSERTEST_H_
