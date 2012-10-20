// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"
#include "webkit/dom_storage/dom_storage_area.h"

namespace {

// The tests server is started separately for each test function (including PRE_
// functions). We need a test server which always uses the same port, so that
// the pages can be accessed with the same URLs after restoring the browser
// session.
class FixedPortTestServer : public net::LocalTestServer {
 public:
  explicit FixedPortTestServer(uint16 port)
      :  LocalTestServer(
             net::TestServer::TYPE_HTTP,
             net::TestServer::kLocalhost,
             FilePath().AppendASCII("chrome/test/data")) {
    BaseTestServer::SetPort(port);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FixedPortTestServer);
};

}  // namespace

class BetterSessionRestoreTest : public InProcessBrowserTest {
 public:
  BetterSessionRestoreTest()
      : test_server_(8001),
        title_pass_(ASCIIToUTF16("PASS")),
        title_storing_(ASCIIToUTF16("STORING")),
        title_error_write_failed_(ASCIIToUTF16("ERROR_WRITE_FAILED")),
        title_error_empty_(ASCIIToUTF16("ERROR_EMPTY")) {
    CHECK(test_server_.Start());
  }
 protected:
  void StoreDataWithPage(const std::string& filename) {
    GURL url = test_server_.GetURL("files/session_restore/" + filename);
    content::WebContents* web_contents =
        chrome::GetActiveWebContents(browser());
    content::TitleWatcher title_watcher(web_contents, title_storing_);
    title_watcher.AlsoWaitForTitle(title_pass_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    ui_test_utils::NavigateToURL(browser(), url);
    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_storing_, final_title);
  }

  void CheckReloadedPage() {
    content::WebContents* web_contents = chrome::GetWebContentsAt(browser(), 0);
    content::TitleWatcher title_watcher(web_contents, title_pass_);
    title_watcher.AlsoWaitForTitle(title_storing_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    // It's possible that the title was already the right one before
    // title_watcher was created.
    if (web_contents->GetTitle() != title_pass_) {
      string16 final_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(title_pass_, final_title);
    }
  }

 private:
  FixedPortTestServer test_server_;
  string16 title_pass_;
  string16 title_storing_;
  string16 title_error_write_failed_;
  string16 title_error_empty_;

  DISALLOW_COPY_AND_ASSIGN(BetterSessionRestoreTest);
};

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, FLAKY_PRE_SessionCookies) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  StoreDataWithPage("session_cookies.html");
}

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, FLAKY_SessionCookies) {
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPage();
}

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, FLAKY_PRE_SessionStorage) {
  // Write the data on disk less lazily.
  dom_storage::DomStorageArea::DisableCommitDelayForTesting();
  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  StoreDataWithPage("session_storage.html");
}

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, FLAKY_SessionStorage) {
  CheckReloadedPage();
}
