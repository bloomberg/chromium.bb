// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_events.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/gfx/point.h"

class AutomationMiscBrowserTest : public InProcessBrowserTest {
 public:
  AutomationMiscBrowserTest() {}

  virtual ~AutomationMiscBrowserTest() {}
};

class MockMouseEventCallback {
 public:
  typedef AutomationMouseEventProcessor::CompletionCallback CompletionCallback;
  typedef AutomationMouseEventProcessor::ErrorCallback ErrorCallback;

  MockMouseEventCallback() {
    completion_callback_ = base::Bind(
        &MockMouseEventCallback::OnSuccess,
        base::Unretained(this));
    error_callback_ = base::Bind(
        &MockMouseEventCallback::OnError,
        base::Unretained(this));
  }
  virtual ~MockMouseEventCallback() { }

  MOCK_METHOD1(OnSuccess, void(const gfx::Point& point));
  MOCK_METHOD1(OnError, void(const automation::Error&));

  const CompletionCallback& completion_callback() {
    return completion_callback_;
  }

  const ErrorCallback& error_callback() {
    return error_callback_;
  }

 private:
  CompletionCallback completion_callback_;
  ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMouseEventCallback);
};

// Tests that automation mouse events are processed correctly by the renderer.
IN_PROC_BROWSER_TEST_F(AutomationMiscBrowserTest, ProcessMouseEvent) {
  MockMouseEventCallback mock;
  EXPECT_CALL(mock,
              OnSuccess(
                  testing::AllOf(
                      testing::Property(&gfx::Point::x, testing::Eq(5)),
                      testing::Property(&gfx::Point::y, testing::Eq(10)))))
      .Times(2);

  content::RenderViewHost* view =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  ASSERT_TRUE(content::ExecuteScript(
      view,
      "window.didClick = false;"
      "document.body.innerHTML ="
      "    '<a style=\\'position:absolute; left:0; top:0\\'>link</a>';"
      "document.querySelector('a').addEventListener('click', function() {"
      "  window.didClick = true;"
      "}, true);"));
  AutomationMouseEvent automation_event;
  automation_event.location_script_chain.push_back(
      ScriptEvaluationRequest("{'x': 5, 'y': 10}", ""));
  WebKit::WebMouseEvent& mouse_event = automation_event.mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  new AutomationMouseEventProcessor(
      view, automation_event, mock.completion_callback(),
      mock.error_callback());
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  new AutomationMouseEventProcessor(
      view, automation_event, mock.completion_callback(),
      mock.error_callback());

  bool did_click = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      view,
      "window.domAutomationController.send(window.didClick);",
      &did_click));
  EXPECT_TRUE(did_click);
}
