// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/web_ui_handler_browsertest.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/test/ui_test_utils.h"

bool WebUIHandlerBrowserTest::Execute(const std::string& js_test) {
  web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      string16(), UTF8ToUTF16(js_test));
  return WaitForResult();
}

void WebUIHandlerBrowserTest::HandlePass(const ListValue* args) {
  test_succeeded_ = true;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();
}

void WebUIHandlerBrowserTest::HandleFail(const ListValue* args) {
  test_succeeded_ = false;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();
}

void WebUIHandlerBrowserTest::RegisterMessages() {
  web_ui_->RegisterMessageCallback("Pass",
      NewCallback(this, &WebUIHandlerBrowserTest::HandlePass));
  web_ui_->RegisterMessageCallback("Fail",
      NewCallback(this, &WebUIHandlerBrowserTest::HandleFail));
}

bool WebUIHandlerBrowserTest::WaitForResult() {
  is_waiting_ = true;
  ui_test_utils::RunMessageLoop();
  is_waiting_ = false;
  return test_succeeded_;
}
