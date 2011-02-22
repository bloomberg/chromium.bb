// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_observer.h"
#include "chrome/browser/tab_contents/tab_specific_content_settings.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/test/test_server.h"

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

class RenderViewHostTest : public InProcessBrowserTest {
 public:
  RenderViewHostTest() : last_execute_id_(0) {}

  void ExecuteJavascriptAndGetValue(const char* script,
                                    ExecuteNotificationObserver* out_result) {
    RenderViewHost* rvh =
        browser()->GetSelectedTabContents()->render_view_host();
    ASSERT_TRUE(rvh);
    int execute_id = rvh->ExecuteJavascriptInWebFrameNotifyResult(
        string16(),
        ASCIIToUTF16(script));
    EXPECT_NE(execute_id, last_execute_id_);
    ExecuteNotificationObserver observer;
    ui_test_utils::RegisterAndWait(
        out_result,
        NotificationType::EXECUTE_JAVASCRIPT_RESULT,
        Source<RenderViewHost>(rvh));
    EXPECT_EQ(execute_id, out_result->id());
    ASSERT_TRUE(out_result->value());
    last_execute_id_ = execute_id;
  }

 private:
  int last_execute_id_;
};


// Makes sure ExecuteJavascriptInWebFrameNotifyResult works.
IN_PROC_BROWSER_TEST_F(RenderViewHostTest,
                       ExecuteJavascriptInWebFrameNotifyResult) {
  ASSERT_TRUE(test_server()->Start());
  GURL empty_url(test_server()->GetURL("files/empty.html"));
  ui_test_utils::NavigateToURL(browser(), empty_url);

  // Execute the script 'true' and make sure we get back true.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("true;", &observer);
    EXPECT_EQ(Value::TYPE_BOOLEAN, observer.value()->GetType());
    bool bool_value;
    EXPECT_TRUE(observer.value()->GetAsBoolean(&bool_value));
    EXPECT_TRUE(bool_value);
  }

  // Execute the script 'false' and make sure we get back false.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("false;", &observer);
    EXPECT_EQ(Value::TYPE_BOOLEAN, observer.value()->GetType());
    bool bool_value;
    EXPECT_TRUE(observer.value()->GetAsBoolean(&bool_value));
    EXPECT_FALSE(bool_value);
  }

  // And now, for something completely different, try a number.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("42;", &observer);
    EXPECT_EQ(Value::TYPE_INTEGER, observer.value()->GetType());
    int int_value;
    EXPECT_TRUE(observer.value()->GetAsInteger(&int_value));
    EXPECT_EQ(42, int_value);
  }

  // Try a floating point number.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("42.2;", &observer);
    EXPECT_EQ(Value::TYPE_DOUBLE, observer.value()->GetType());
    double double_value;
    EXPECT_TRUE(observer.value()->GetAsDouble(&double_value));
    EXPECT_EQ(42.2, double_value);
  }

  // Let's check out string.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("\"something completely different\";",
                                 &observer);
    EXPECT_EQ(Value::TYPE_STRING, observer.value()->GetType());
    std::string string_value;
    EXPECT_TRUE(observer.value()->GetAsString(&string_value));
    EXPECT_EQ(std::string("something completely different"), string_value);
  }

  // Regular expressions might be fun.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("/finder.*foo/g;", &observer);
    EXPECT_EQ(Value::TYPE_STRING, observer.value()->GetType());
    std::string string_value;
    EXPECT_TRUE(observer.value()->GetAsString(&string_value));
    EXPECT_EQ(std::string("/finder.*foo/g"), string_value);
  }

  // Let's test some date conversions.  First up, epoch.  Can't use 0 because
  // that means uninitialized, so use the next best thing.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("new Date(1);", &observer);
    EXPECT_EQ(Value::TYPE_DOUBLE, observer.value()->GetType());
    double date_seconds;
    EXPECT_TRUE(observer.value()->GetAsDouble(&date_seconds));

    base::Time time = base::Time::FromDoubleT(date_seconds);

    base::Time::Exploded time_exploded;
    time.UTCExplode(&time_exploded);
    EXPECT_EQ(1970, time_exploded.year);
    EXPECT_EQ(1, time_exploded.month);
    EXPECT_EQ(1, time_exploded.day_of_month);
    EXPECT_EQ(0, time_exploded.hour);
    EXPECT_EQ(0, time_exploded.minute);
    EXPECT_EQ(0, time_exploded.second);
  }

  // Test date with a real date input.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("new Date(Date.UTC(2006, 7, 16, 12, 0, 15));",
                                 &observer);
    EXPECT_EQ(Value::TYPE_DOUBLE, observer.value()->GetType());
    double date_seconds;
    EXPECT_TRUE(observer.value()->GetAsDouble(&date_seconds));

    base::Time time = base::Time::FromDoubleT(date_seconds);

    base::Time::Exploded time_exploded;
    time.UTCExplode(&time_exploded);
    EXPECT_EQ(2006, time_exploded.year);
    // Subtle; 0 based in JS, 1 based in base::Time:
    EXPECT_EQ(8, time_exploded.month);
    EXPECT_EQ(16, time_exploded.day_of_month);
    EXPECT_EQ(12, time_exploded.hour);
    EXPECT_EQ(0, time_exploded.minute);
    EXPECT_EQ(15, time_exploded.second);
  }

  // And something more complicated - get an array back as a list.
  {
    ExecuteNotificationObserver observer;
    ExecuteJavascriptAndGetValue("new Array(\"one\", 2, false);", &observer);
    EXPECT_EQ(Value::TYPE_LIST, observer.value()->GetType());
    ListValue* list_value;
    EXPECT_TRUE(observer.value()->GetAsList(&list_value));
    EXPECT_EQ(3U, list_value->GetSize());
    Value* value;
    EXPECT_TRUE(list_value->Get(0, &value));
    EXPECT_EQ(Value::TYPE_STRING, value->GetType());
    EXPECT_TRUE(list_value->Get(1, &value));
    EXPECT_EQ(Value::TYPE_INTEGER, value->GetType());
    EXPECT_TRUE(list_value->Get(2, &value));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
  }
}

// Regression test for http://crbug.com/63649.
IN_PROC_BROWSER_TEST_F(RenderViewHostTest, RedirectLoopCookies) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_url = test_server()->GetURL("files/redirect-loop.html");

  browser()->profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_BLOCK);

  ui_test_utils::NavigateToURL(browser(), test_url);

  TabContents* tab_contents = browser()->GetSelectedTabContents();
  ASSERT_EQ(UTF8ToUTF16(test_url.spec() + " failed to load"),
            tab_contents->GetTitle());

  EXPECT_TRUE(tab_contents->GetTabSpecificContentSettings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_COOKIES));
}

class RenderViewHostTestTabContentsObserver : public TabContentsObserver {
 public:
  explicit RenderViewHostTestTabContentsObserver(TabContents* tab_contents)
      : TabContentsObserver(tab_contents),
        navigation_count_(0) {}
  virtual ~RenderViewHostTestTabContentsObserver() {}

  virtual void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params) {
    observed_socket_address_ = params.socket_address;
    ++navigation_count_;
  }

  const net::HostPortPair& observed_socket_address() const {
    return observed_socket_address_;
  }

  int navigation_count() const { return navigation_count_; }

 private:
  net::HostPortPair observed_socket_address_;
  int navigation_count_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestTabContentsObserver);
};

IN_PROC_BROWSER_TEST_F(RenderViewHostTest, FrameNavigateSocketAddress) {
  ASSERT_TRUE(test_server()->Start());
  RenderViewHostTestTabContentsObserver observer(
      browser()->GetSelectedTabContents());

  GURL test_url = test_server()->GetURL("files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  EXPECT_EQ(test_server()->host_port_pair().ToString(),
            observer.observed_socket_address().ToString());
  EXPECT_EQ(1, observer.navigation_count());
}
