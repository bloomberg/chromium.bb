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
