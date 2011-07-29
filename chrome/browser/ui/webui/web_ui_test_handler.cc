// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_test_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_details.h"
#include "content/common/notification_registrar.h"

bool WebUITestHandler::RunJavascript(const std::string& js_test, bool is_test) {
  if (is_test) {
    NotificationRegistrar notification_registrar;
    notification_registrar.Add(
        this, content::NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,
        Source<RenderViewHost>(web_ui_->GetRenderViewHost()));
    web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrameNotifyResult(
        string16(), UTF8ToUTF16(js_test));
    return WaitForResult();
  } else {
    web_ui_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
        string16(), UTF8ToUTF16(js_test));
    return true;
  }
}

void WebUITestHandler::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  // Quit the message loop if we were waiting so Waiting process can get result
  // or error. To ensure this gets done, do this before ASSERT* calls.
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();

  SCOPED_TRACE("WebUITestHandler::Observe");
  Value* value = Details<std::pair<int, Value*> >(details)->second;
  ListValue* list_value;
  ASSERT_TRUE(value->GetAsList(&list_value));
  ASSERT_TRUE(list_value->GetBoolean(0, &test_succeeded_));
  if (!test_succeeded_) {
    std::string message;
    ASSERT_TRUE(list_value->GetString(1, &message));
    LOG(ERROR) << message;
  }
}

bool WebUITestHandler::WaitForResult() {
  is_waiting_ = true;
  ui_test_utils::RunMessageLoop();
  is_waiting_ = false;
  return test_succeeded_;
}
