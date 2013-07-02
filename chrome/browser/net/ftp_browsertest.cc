// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FtpBrowserTest : public InProcessBrowserTest {
 public:
  FtpBrowserTest()
      : ftp_server_(net::SpawnedTestServer::TYPE_FTP,
                    net::SpawnedTestServer::kLocalhost,
                    base::FilePath()) {
  }

 protected:
  net::SpawnedTestServer ftp_server_;
};

IN_PROC_BROWSER_TEST_F(FtpBrowserTest, DirectoryListing) {
  ASSERT_TRUE(ftp_server_.Start());
  ui_test_utils::NavigateToURL(browser(), ftp_server_.GetURL("/"));
  // TODO(phajdan.jr): test more things.
}
