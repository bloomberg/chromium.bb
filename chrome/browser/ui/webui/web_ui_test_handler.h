// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_UI_TEST_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_UI_TEST_HANDLER_H_
#pragma once

#include "base/string16.h"
#include "content/browser/webui/web_ui.h"
#include "content/common/notification_observer.h"

// This class registers test framework specific handlers on WebUI objects.
class WebUITestHandler : public WebUIMessageHandler,
                         public NotificationObserver {
 public:
  // Sends a message through |preload_host| with the |js_text| to preload at the
  // appropriate time before the onload call is made.
  void PreloadJavaScript(const string16& js_text,
                         RenderViewHost* preload_host);

  // Runs |js_text| in this object's WebUI frame. Does not wait for any result.
  void RunJavaScript(const string16& js_text);

  // Runs |js_text| in this object's WebUI frame. Waits for result, logging an
  // error message on failure. Returns test pass/fail.
  bool RunJavaScriptTestWithResult(const string16& js_text);

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
