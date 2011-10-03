// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_test_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_registrar.h"

WebUITestHandler::WebUITestHandler()
    : test_done_(false),
      test_succeeded_(false),
      run_test_done_(false),
      run_test_succeeded_(false),
      is_waiting_(false) {
}

void WebUITestHandler::PreloadJavaScript(const string16& js_text,
                                         RenderViewHost* preload_host) {
  DCHECK(preload_host);
  preload_host->Send(new ChromeViewMsg_WebUIJavaScript(
      preload_host->routing_id(), string16(), js_text, 0,
      false));
}

void WebUITestHandler::RunJavaScript(const string16& js_text) {
  web_ui_->tab_contents()->render_view_host()->ExecuteJavascriptInWebFrame(
      string16(), js_text);
}

bool WebUITestHandler::RunJavaScriptTestWithResult(const string16& js_text) {
  test_succeeded_ = false;
  run_test_succeeded_ = false;
  RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
  NotificationRegistrar notification_registrar;
  notification_registrar.Add(
      this, content::NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,
      Source<RenderViewHost>(rvh));
  rvh->ExecuteJavascriptInWebFrameNotifyResult(string16(), js_text);
  return WaitForResult();
}

void WebUITestHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("testResult",
      base::Bind(&WebUITestHandler::HandleTestResult, base::Unretained(this)));
}

void WebUITestHandler::HandleTestResult(const ListValue* test_result) {
  // Quit the message loop if |is_waiting_| so waiting process can get result or
  // error. To ensure this gets done, do this before ASSERT* calls.
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();

  SCOPED_TRACE("WebUITestHandler::HandleTestResult");

  EXPECT_FALSE(test_done_);
  test_done_ = true;
  test_succeeded_ = false;

  ASSERT_TRUE(test_result->GetBoolean(0, &test_succeeded_));
  if (!test_succeeded_) {
    std::string message;
    ASSERT_TRUE(test_result->GetString(1, &message));
    LOG(ERROR) << message;
  }
}

void WebUITestHandler::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  // Quit the message loop if |is_waiting_| so waiting process can get result or
  // error. To ensure this gets done, do this before ASSERT* calls.
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();

  ASSERT_EQ(content::NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT, type);

  SCOPED_TRACE("WebUITestHandler::Observe");

  EXPECT_FALSE(run_test_done_);
  run_test_done_ = true;
  run_test_succeeded_ = false;

  Value* value = Details<std::pair<int, Value*> >(details)->second;
  ASSERT_TRUE(value->GetAsBoolean(&run_test_succeeded_));
}

bool WebUITestHandler::WaitForResult() {
  SCOPED_TRACE("WebUITestHandler::WaitForResult");
  test_done_ = false;
  run_test_done_ = false;
  is_waiting_ = true;

  // Either sync test completion or the testDone() will cause message loop
  // to quit.
  ui_test_utils::RunMessageLoop();

  // Run a second message loop when not |run_test_done_| so that the sync test
  // completes, or |run_test_succeeded_| but not |test_done_| so async tests
  // complete.
  if (!run_test_done_ || (run_test_succeeded_ && !test_done_)) {
    ui_test_utils::RunMessageLoop();
  }

  is_waiting_ = false;

  // To succeed the test must execute as well as pass the test.
  return run_test_succeeded_ && test_succeeded_;
}
