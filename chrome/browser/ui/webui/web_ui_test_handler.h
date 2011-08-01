// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_UI_TEST_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_UI_TEST_HANDLER_H_
#pragma once

#include <string>

#include "content/browser/webui/web_ui.h"
#include "content/common/notification_observer.h"

// This class registers test framework specific handlers on WebUI objects.
class WebUITestHandler : public WebUIMessageHandler,
                         public NotificationObserver {
 public:
  // Runs a string of javascript. Returns pass fail.
  bool RunJavascript(const std::string& js_test, bool is_test);

 private:
  // WebUIMessageHandler overrides.
  // Add test handlers to the current WebUI object.
  virtual void RegisterMessages() OVERRIDE {}

  // From NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Runs a message loop until test finishes.  Returns the result of the test.
  bool WaitForResult();

  // Pass fail result of current tests.
  bool test_succeeded_;

  // Waiting for a test to finish.
  bool is_waiting_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_UI_TEST_HANDLER_H_
