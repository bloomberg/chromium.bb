// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

class FtpBrowserTest : public InProcessBrowserTest {
 public:
  FtpBrowserTest() : server_(FTPTestServer::CreateServer(L"")) {
  }

 protected:
  scoped_refptr<FTPTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(FtpBrowserTest, DirectoryListing) {
  ASSERT_TRUE(NULL != server_.get());
  ui_test_utils::NavigateToURL(browser(), server_->TestServerPage("/"));
  // TODO(phajdan.jr): test more things.
}
