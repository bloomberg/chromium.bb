// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class FtpBrowserTest : public InProcessBrowserTest {
 public:
  FtpBrowserTest() : ftp_server_(net::TestServer::TYPE_FTP, FilePath()) {
  }

 protected:
  net::TestServer ftp_server_;
};

IN_PROC_BROWSER_TEST_F(FtpBrowserTest, DirectoryListing) {
  ASSERT_TRUE(ftp_server_.Start());
  ui_test_utils::NavigateToURL(browser(), ftp_server_.GetURL("/"));
  // TODO(phajdan.jr): test more things.
}
