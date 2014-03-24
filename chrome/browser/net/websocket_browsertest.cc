// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace {

class WebSocketBrowserTest : public InProcessBrowserTest {
 public:
  WebSocketBrowserTest()
    : ws_server_(net::SpawnedTestServer::TYPE_WS,
                 net::SpawnedTestServer::kLocalhost,
                 net::GetWebSocketTestDataDirectory()),
      wss_server_(net::SpawnedTestServer::TYPE_WSS,
                  SSLOptions(SSLOptions::CERT_OK),
                  net::GetWebSocketTestDataDirectory()) {
  }

 protected:
  net::SpawnedTestServer ws_server_;
  net::SpawnedTestServer wss_server_;

 private:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  DISALLOW_COPY_AND_ASSIGN(WebSocketBrowserTest);
};

// Test that the browser can handle a WebSocket frame split into multiple TCP
// segments.
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketSplitSegments) {
  // Launch a WebSocket server.
  ASSERT_TRUE(ws_server_.Start());

  // Setup page title observer.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a HTTP page for testing.
  std::string scheme("http");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      ws_server_.GetURL(
          "split_packet_check.html").ReplaceComponents(replacements));

  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(EqualsASCII(result, "PASS"));
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, SecureWebSocketSplitRecords) {
  // Launch a secure WebSocket server.
  ASSERT_TRUE(wss_server_.Start());

  // Setup page title observer.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a HTTPS page for testing.
  std::string scheme("https");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      wss_server_.GetURL(
          "split_packet_check.html").ReplaceComponents(replacements));

  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(EqualsASCII(result, "PASS"));
}

}  // namespace
