// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_test_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"

bool WebUITestHandler::RunJavascript(const std::string& js_test,
                                            bool is_test) {
  web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      string16(), UTF8ToUTF16(js_test));

  if (is_test)
    return WaitForResult();
  else
    return true;
}

void WebUITestHandler::HandlePass(const ListValue* args) {
  test_succeeded_ = true;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();
}

void WebUITestHandler::HandleFail(const ListValue* args) {
  test_succeeded_ = false;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();

  std::string message;
  ASSERT_TRUE(args->GetString(0, &message));
  LOG(INFO) << message;
}

void WebUITestHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("Pass",
      NewCallback(this, &WebUITestHandler::HandlePass));
  web_ui_->RegisterMessageCallback("Fail",
      NewCallback(this, &WebUITestHandler::HandleFail));
}

bool WebUITestHandler::WaitForResult() {
  is_waiting_ = true;
  ui_test_utils::RunMessageLoop();
  is_waiting_ = false;
  return test_succeeded_;
}
