// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_HANDLER_BROWSERTEST_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_HANDLER_BROWSERTEST_H_
#pragma once

#include <string>

#include "chrome/browser/dom_ui/web_ui.h"

// This class registers test framework specific handlers on WebUI objects.
class DOMUITestHandler : public WebUIMessageHandler {
 public:
  // Executes a string of javascript.  Returns pass fail.
  bool Execute(const std::string& js_test);

 protected:
  // WebUI handlers which deliver results to any waiting message loops.
  // |args| is currently ignored.
  void HandlePass(const ListValue* args);
  void HandleFail(const ListValue* args);

  // WebUIMessageHandler overrides.
  // Add test handlers to the current WebUI object.
  virtual void RegisterMessages();

 private:
  // Runs a message loop until test finishes.  Returns the result of the test.
  bool WaitForResult();

  // Pass fail result of current tests.
  bool test_succeeded_;

  // Waiting for a test to finish.
  bool is_waiting_;
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_HANDLER_BROWSERTEST_H_
