// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_handler_browsertest.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/test/ui_test_utils.h"

bool DOMUITestHandler::Execute(const std::string& js_test) {
  dom_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      string16(), UTF8ToUTF16(js_test));
  return WaitForResult();
}

void DOMUITestHandler::HandlePass(const ListValue* args) {
  test_succeeded_ = true;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();
}

void DOMUITestHandler::HandleFail(const ListValue* args) {
  test_succeeded_ = false;
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();
}

void DOMUITestHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("Pass",
      NewCallback(this, &DOMUITestHandler::HandlePass));
  dom_ui_->RegisterMessageCallback("Fail",
      NewCallback(this, &DOMUITestHandler::HandleFail));
}

bool DOMUITestHandler::WaitForResult() {
  is_waiting_ = true;
  ui_test_utils::RunMessageLoop();
  is_waiting_ = false;
  return test_succeeded_;
}
