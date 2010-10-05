// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/test/test_server.h"

typedef InProcessBrowserTest RenderViewHostTest;

typedef std::pair<int, Value*> ExecuteDetailType;

namespace {

// NotificationObserver used to listen for EXECUTE_JAVASCRIPT_RESULT
// notifications.
class ExecuteNotificationObserver : public NotificationObserver {
 public:
  ExecuteNotificationObserver() : id_(0) {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    id_ = (static_cast<Details<ExecuteDetailType > >(details))->first;
    Value* value = (static_cast<Details<ExecuteDetailType > >(details))->second;
    if (value)
      value_.reset(value->DeepCopy());
    MessageLoopForUI::current()->Quit();
  }

  int id() const { return id_; }

  Value* value() const { return value_.get(); }

 private:
  int id_;
  scoped_ptr<Value> value_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteNotificationObserver);
};

}  // namespace

// Makes sure ExecuteJavascriptInWebFrameNotifyResult works.
IN_PROC_BROWSER_TEST_F(RenderViewHostTest,
                       ExecuteJavascriptInWebFrameNotifyResult) {
  ASSERT_TRUE(test_server()->Start());
  GURL empty_url(test_server()->GetURL("files/empty.html"));
  ui_test_utils::NavigateToURL(browser(), empty_url);
  RenderViewHost* rvh = browser()->GetSelectedTabContents()->render_view_host();
  ASSERT_TRUE(rvh);

  // Execute the script 'true' and make sure we get back true.
  int execute_id = rvh->ExecuteJavascriptInWebFrameNotifyResult(
      string16(),
      ASCIIToUTF16("true;"));
  {
    ExecuteNotificationObserver observer;
    ui_test_utils::RegisterAndWait(
        &observer,
        NotificationType::EXECUTE_JAVASCRIPT_RESULT,
        Source<RenderViewHost>(rvh));
    EXPECT_EQ(execute_id, observer.id());
    ASSERT_TRUE(observer.value());
    bool bool_value;
    ASSERT_TRUE(observer.value()->GetAsBoolean(&bool_value));
    EXPECT_TRUE(bool_value);
  }

  // Execute the script 'false' and make sure we get back false.
  int execute_id2 = rvh->ExecuteJavascriptInWebFrameNotifyResult(
      string16(),
      ASCIIToUTF16("false;"));
  // The ids should change.
  EXPECT_NE(execute_id, execute_id2);
  {
    ExecuteNotificationObserver observer;
    ui_test_utils::RegisterAndWait(
        &observer,
        NotificationType::EXECUTE_JAVASCRIPT_RESULT,
        Source<RenderViewHost>(rvh));
    EXPECT_EQ(execute_id2, observer.id());
    ASSERT_TRUE(observer.value());
    bool bool_value;
    ASSERT_TRUE(observer.value()->GetAsBoolean(&bool_value));
    EXPECT_FALSE(bool_value);
  }
}
