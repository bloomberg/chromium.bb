// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

namespace content {

class RenderViewHostTest : public ContentBrowserTest {
 public:
  RenderViewHostTest() {}
};

IN_PROC_BROWSER_TEST_F(RenderViewHostTest,
                       ExecuteJavascriptAndGetValue) {
  ASSERT_TRUE(test_server()->Start());
  GURL empty_url(test_server()->GetURL("files/simple_page.html"));
  NavigateToURL(shell(), empty_url);

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      shell()->web_contents()->GetRenderViewHost());

  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("!false;")));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
    bool bool_value;
    EXPECT_TRUE(value->GetAsBoolean(&bool_value));
    EXPECT_TRUE(bool_value);
  }

  // Execute the script 'true' and make sure we get back true.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("true;")));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
    bool bool_value;
    EXPECT_TRUE(value->GetAsBoolean(&bool_value));
    EXPECT_TRUE(bool_value);
  }

  // Execute the script 'false' and make sure we get back false.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("false;")));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
    bool bool_value;
    EXPECT_TRUE(value->GetAsBoolean(&bool_value));
    EXPECT_FALSE(bool_value);
  }

  // And now, for something completely different, try a number.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("42;")));
    EXPECT_EQ(Value::TYPE_INTEGER, value->GetType());
    int int_value;
    EXPECT_TRUE(value->GetAsInteger(&int_value));
    EXPECT_EQ(42, int_value);
  }

  // Try a floating point number.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("42.2;")));
    EXPECT_EQ(Value::TYPE_DOUBLE, value->GetType());
    double double_value;
    EXPECT_TRUE(value->GetAsDouble(&double_value));
    EXPECT_EQ(42.2, double_value);
  }

  // Let's check out string.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("\"something completely different\";")));
    EXPECT_EQ(Value::TYPE_STRING, value->GetType());
    std::string string_value;
    EXPECT_TRUE(value->GetAsString(&string_value));
    EXPECT_EQ(std::string("something completely different"), string_value);
  }

  // Regular expressions might be fun.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("/finder.*foo/g;")));
    EXPECT_EQ(Value::TYPE_STRING, value->GetType());
    std::string string_value;
    EXPECT_TRUE(value->GetAsString(&string_value));
    EXPECT_EQ(std::string("/finder.*foo/g"), string_value);
  }

  // Let's test some date conversions.  First up, epoch.  Can't use 0 because
  // that means uninitialized, so use the next best thing.
  {
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("new Date(1);")));
    EXPECT_EQ(Value::TYPE_DOUBLE, value->GetType());
    double date_seconds;
    EXPECT_TRUE(value->GetAsDouble(&date_seconds));

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
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("new Date(Date.UTC(2006, 7, 16, 12, 0, 15));")));
    EXPECT_EQ(Value::TYPE_DOUBLE, value->GetType());
    double date_seconds;
    EXPECT_TRUE(value->GetAsDouble(&date_seconds));

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
    scoped_ptr<Value> value(
        rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("new Array(\"one\", 2, false);")));
    EXPECT_EQ(Value::TYPE_LIST, value->GetType());
    ListValue* list_value;
    EXPECT_TRUE(value->GetAsList(&list_value));
    EXPECT_EQ(3U, list_value->GetSize());
    Value* element_value;
    EXPECT_TRUE(list_value->Get(0, &element_value));
    EXPECT_EQ(Value::TYPE_STRING, element_value->GetType());
    EXPECT_TRUE(list_value->Get(1, &element_value));
    EXPECT_EQ(Value::TYPE_INTEGER, element_value->GetType());
    EXPECT_TRUE(list_value->Get(2, &element_value));
    EXPECT_EQ(Value::TYPE_BOOLEAN, element_value->GetType());
  }
}

class RenderViewHostTestWebContentsObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostTestWebContentsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_count_(0) {}
  virtual ~RenderViewHostTestWebContentsObserver() {}

  virtual void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) OVERRIDE {
    observed_socket_address_ = params.socket_address;
    base_url_ = params.base_url;
    ++navigation_count_;
  }

  const net::HostPortPair& observed_socket_address() const {
    return observed_socket_address_;
  }

  GURL base_url() const {
    return base_url_;
  }

  int navigation_count() const { return navigation_count_; }

 private:
  net::HostPortPair observed_socket_address_;
  GURL base_url_;
  int navigation_count_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestWebContentsObserver);
};

IN_PROC_BROWSER_TEST_F(RenderViewHostTest, FrameNavigateSocketAddress) {
  ASSERT_TRUE(test_server()->Start());
  RenderViewHostTestWebContentsObserver observer(shell()->web_contents());

  GURL test_url = test_server()->GetURL("files/simple_page.html");
  NavigateToURL(shell(), test_url);

  EXPECT_EQ(test_server()->host_port_pair().ToString(),
            observer.observed_socket_address().ToString());
  EXPECT_EQ(1, observer.navigation_count());
}

IN_PROC_BROWSER_TEST_F(RenderViewHostTest, BaseURLParam) {
  ASSERT_TRUE(test_server()->Start());
  RenderViewHostTestWebContentsObserver observer(shell()->web_contents());

  // Base URL is not set if it is the same as the URL.
  GURL test_url = test_server()->GetURL("files/simple_page.tml");
  NavigateToURL(shell(), test_url);
  EXPECT_TRUE(observer.base_url().is_empty());
  EXPECT_EQ(1, observer.navigation_count());

  // But should be set to the original page when reading MHTML.
  test_url = net::FilePathToFileURL(test_server()->document_root().Append(
      FILE_PATH_LITERAL("google.mht")));
  NavigateToURL(shell(), test_url);
  EXPECT_EQ("http://www.google.com/", observer.base_url().spec());
}

}  // namespace content
