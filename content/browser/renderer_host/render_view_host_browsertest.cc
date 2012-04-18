// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

using content::RenderViewHostImpl;
using content::WebContents;

class RenderViewHostTest : public InProcessBrowserTest {
 public:
  RenderViewHostTest() {}
};


IN_PROC_BROWSER_TEST_F(RenderViewHostTest,
                       ExecuteJavascriptAndGetValue) {
  ASSERT_TRUE(test_server()->Start());
  GURL empty_url(test_server()->GetURL("files/empty.html"));
  ui_test_utils::NavigateToURL(browser(), empty_url);

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      browser()->GetSelectedWebContents()->GetRenderViewHost());

  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
                                                     ASCIIToUTF16("!false;"));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
    bool bool_value;
    EXPECT_TRUE(value->GetAsBoolean(&bool_value));
    EXPECT_TRUE(bool_value);
  }

  // Execute the script 'true' and make sure we get back true.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
                                                     ASCIIToUTF16("true;"));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
    bool bool_value;
    EXPECT_TRUE(value->GetAsBoolean(&bool_value));
    EXPECT_TRUE(bool_value);
  }

  // Execute the script 'false' and make sure we get back false.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
                                                     ASCIIToUTF16("false;"));
    EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
    bool bool_value;
    EXPECT_TRUE(value->GetAsBoolean(&bool_value));
    EXPECT_FALSE(bool_value);
  }

  // And now, for something completely different, try a number.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
                                                     ASCIIToUTF16("42;"));
    EXPECT_EQ(Value::TYPE_INTEGER, value->GetType());
    int int_value;
    EXPECT_TRUE(value->GetAsInteger(&int_value));
    EXPECT_EQ(42, int_value);
  }

  // Try a floating point number.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
                                                     ASCIIToUTF16("42.2;"));
    EXPECT_EQ(Value::TYPE_DOUBLE, value->GetType());
    double double_value;
    EXPECT_TRUE(value->GetAsDouble(&double_value));
    EXPECT_EQ(42.2, double_value);
  }

  // Let's check out string.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("\"something completely different\";"));
    EXPECT_EQ(Value::TYPE_STRING, value->GetType());
    std::string string_value;
    EXPECT_TRUE(value->GetAsString(&string_value));
    EXPECT_EQ(std::string("something completely different"), string_value);
  }

  // Regular expressions might be fun.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("/finder.*foo/g;"));
    EXPECT_EQ(Value::TYPE_STRING, value->GetType());
    std::string string_value;
    EXPECT_TRUE(value->GetAsString(&string_value));
    EXPECT_EQ(std::string("/finder.*foo/g"), string_value);
  }

  // Let's test some date conversions.  First up, epoch.  Can't use 0 because
  // that means uninitialized, so use the next best thing.
  {
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("new Date(1);"));
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
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("new Date(Date.UTC(2006, 7, 16, 12, 0, 15));"));
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
    Value* value = rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("new Array(\"one\", 2, false);"));
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

class RenderViewHostTestWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit RenderViewHostTestWebContentsObserver(WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        navigation_count_(0) {}
  virtual ~RenderViewHostTestWebContentsObserver() {}

  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE {
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
  RenderViewHostTestWebContentsObserver observer(
      browser()->GetSelectedWebContents());

  GURL test_url = test_server()->GetURL("files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  EXPECT_EQ(test_server()->host_port_pair().ToString(),
            observer.observed_socket_address().ToString());
  EXPECT_EQ(1, observer.navigation_count());
}

IN_PROC_BROWSER_TEST_F(RenderViewHostTest, BaseURLParam) {
  ASSERT_TRUE(test_server()->Start());
  RenderViewHostTestWebContentsObserver observer(
      browser()->GetSelectedWebContents());

  // Base URL is not set if it is the same as the URL.
  GURL test_url = test_server()->GetURL("files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);
  EXPECT_TRUE(observer.base_url().is_empty());
  EXPECT_EQ(1, observer.navigation_count());

  // But should be set to the original page when reading MHTML.
  test_url = net::FilePathToFileURL(test_server()->document_root().Append(
      FILE_PATH_LITERAL("google.mht")));
  ui_test_utils::NavigateToURL(browser(), test_url);
  EXPECT_EQ("http://www.google.com/", observer.base_url().spec());
}

// Test that a hung renderer is killed after navigating away during cross-site
// navigation.
// Disabling until actual process termination is enabled back. crbug.com/104346
IN_PROC_BROWSER_TEST_F(RenderViewHostTest,
                       DISABLED_UnresponsiveCrossSiteNavigation) {
  WebContents* web_contents = NULL;
  WebContents* web_contents_2 = NULL;
  content::RenderProcessHost* rph = NULL;
  base::ProcessHandle process;
  FilePath doc_root;

  doc_root = doc_root.Append(FILE_PATH_LITERAL("content"));
  doc_root = doc_root.Append(FILE_PATH_LITERAL("test"));
  doc_root = doc_root.Append(FILE_PATH_LITERAL("data"));

  // Start two servers to enable cross-site navigations.
  net::TestServer server(net::TestServer::TYPE_HTTP,
      net::TestServer::kLocalhost, doc_root);
  ASSERT_TRUE(server.Start());
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost, doc_root);
  ASSERT_TRUE(https_server.Start());

  GURL infinite_beforeunload_url(
      server.GetURL("files/infinite_beforeunload.html"));
  GURL infinite_unload_url(server.GetURL("files/infinite_unload.html"));
  GURL same_process_url(server.GetURL("files/english_page.html"));
  GURL new_process_url(https_server.GetURL("files/english_page.html"));

  // Navigate the tab to the page which will lock up the process when we
  // navigate away from it.
  ui_test_utils::NavigateToURL(browser(), infinite_beforeunload_url);
  web_contents = browser()->GetWebContentsAt(0);
  rph = web_contents->GetRenderProcessHost();
  EXPECT_EQ(web_contents->GetURL(), infinite_beforeunload_url);

  // Remember the process prior to navigation, as we expect it to get killed.
  process = rph->GetHandle();
  ASSERT_TRUE(process);

  {
    ui_test_utils::WindowedNotificationObserver process_exit_observer(
        content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
        content::NotificationService::AllSources());
    ui_test_utils::WindowedNotificationObserver process_hang_observer(
        content::NOTIFICATION_RENDERER_PROCESS_HANG,
        content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), new_process_url);
    process_hang_observer.Wait();
    process_exit_observer.Wait();

    // Since the process is killed during the navigation, we don't expect the
    // renderer host to have a connection to it.
    EXPECT_FALSE(rph->HasConnection());
  }

  ui_test_utils::NavigateToURL(browser(), same_process_url);
  web_contents = browser()->GetWebContentsAt(0);
  rph = web_contents->GetRenderProcessHost();
  ASSERT_TRUE(web_contents != NULL);
  EXPECT_EQ(web_contents->GetURL(), same_process_url);

  // Now, let's open another tab with the unresponsive page, which will cause
  // the previous page and the unresponsive one to use the same process.
  ui_test_utils::NavigateToURLWithDisposition(browser(),
      infinite_unload_url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(browser()->tab_count(), 2);
  web_contents_2 = browser()->GetWebContentsAt(1);
  ASSERT_TRUE(web_contents_2 != NULL);
  EXPECT_EQ(web_contents_2->GetURL(), infinite_unload_url);
  EXPECT_EQ(rph, web_contents_2->GetRenderProcessHost());

  process = rph->GetHandle();
  ASSERT_TRUE(process);

  // Navigating to the cross site URL will not kill the process, since it will
  // have more than one tab using it. Kill it to confirm that it is still there,
  // as well as finish the test faster.
  {
    ui_test_utils::WindowedNotificationObserver process_exit_observer(
        content::NOTIFICATION_RENDERER_PROCESS_HANG,
        content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), new_process_url);
    process_exit_observer.Wait();
  }

  EXPECT_TRUE(base::KillProcess(process, 2, false));
}

// Test that a hung renderer is killed when we are closing the page.
// Disabling until actual process termination is enabled back. crbug.com/104346
IN_PROC_BROWSER_TEST_F(RenderViewHostTest, DISABLED_UnresponsiveClosePage) {
  WebContents* web_contents = NULL;
  FilePath doc_root;

  doc_root = doc_root.Append(FILE_PATH_LITERAL("content"));
  doc_root = doc_root.Append(FILE_PATH_LITERAL("test"));
  doc_root = doc_root.Append(FILE_PATH_LITERAL("data"));

  net::TestServer server(net::TestServer::TYPE_HTTP,
      net::TestServer::kLocalhost, doc_root);
  ASSERT_TRUE(server.Start());
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost, doc_root);
  ASSERT_TRUE(https_server.Start());

  GURL infinite_beforeunload_url(
      server.GetURL("files/infinite_beforeunload.html"));
  GURL infinite_unload_url(server.GetURL("files/infinite_unload.html"));
  GURL new_process_url(https_server.GetURL("files/english_page.html"));

  ui_test_utils::NavigateToURL(browser(), new_process_url);

  // Navigate a tab to a page which will spin into an infinite loop in the
  // beforeunload handler, tying up the process when we close the tab.
  ui_test_utils::NavigateToURLWithDisposition(browser(),
      infinite_beforeunload_url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  web_contents = browser()->GetWebContentsAt(1);
  {
    ui_test_utils::WindowedNotificationObserver process_exit_observer(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::NotificationService::AllSources());
    browser()->CloseTabContents(web_contents);
    process_exit_observer.Wait();
  }

  // Navigate a tab to a page which will spin into an infinite loop in the
  // unload handler, tying up the process when we close the tab.
  ui_test_utils::NavigateToURLWithDisposition(browser(),
      infinite_unload_url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  web_contents = browser()->GetWebContentsAt(1);
  {
    ui_test_utils::WindowedNotificationObserver process_exit_observer(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::NotificationService::AllSources());
    browser()->CloseTabContents(web_contents);
    process_exit_observer.Wait();
  }
}
