// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
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
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"
#include "webkit/dom_storage/dom_storage_area.h"

namespace {

// We need to serve the test files so that PRE_Test and Test can access the same
// page using the same URL. In addition, perceived security origin of the page
// needs to stay the same, so e.g., redirecting the URL requests doesn't
// work. (If we used a test server, the PRE_Test and Test would have separate
// instances running on separate ports.)

static base::LazyInstance<std::map<std::string, std::string> > file_contents =
    LAZY_INSTANCE_INITIALIZER;

net::URLRequestJob* URLRequestFaker(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  return new net::URLRequestTestJob(
      request, network_delegate, net::URLRequestTestJob::test_headers(),
      file_contents.Get()[request->url().path()], true);
}

}  // namespace

class BetterSessionRestoreTest : public InProcessBrowserTest {
 public:
  BetterSessionRestoreTest()
      : title_pass_(ASCIIToUTF16("PASS")),
        title_storing_(ASCIIToUTF16("STORING")),
        title_error_write_failed_(ASCIIToUTF16("ERROR_WRITE_FAILED")),
        title_error_empty_(ASCIIToUTF16("ERROR_EMPTY")),
        fake_server_address_("http://www.test.com/"),
        test_path_("session_restore/") {
    // Set up the URL request filtering.
    std::vector<std::string> test_files;
    test_files.push_back("common.js");
    test_files.push_back("session_cookies.html");
    test_files.push_back("session_storage.html");
    FilePath test_file_dir;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &test_file_dir));
    test_file_dir =
        test_file_dir.AppendASCII("chrome/test/data").AppendASCII(test_path_);

    for (std::vector<std::string>::const_iterator it = test_files.begin();
         it != test_files.end(); ++it) {
      FilePath path = test_file_dir.AppendASCII(*it);
      std::string contents;
      CHECK(file_util::ReadFileToString(path, &contents));
      file_contents.Get()["/" + test_path_ + *it] = contents;
      net::URLRequestFilter::GetInstance()->AddUrlHandler(
          GURL(fake_server_address_ + test_path_ + *it),
          &URLRequestFaker);
    }
  }

 protected:
  void StoreDataWithPage(const std::string& filename) {
    content::WebContents* web_contents =
        chrome::GetActiveWebContents(browser());
    content::TitleWatcher title_watcher(web_contents, title_storing_);
    title_watcher.AlsoWaitForTitle(title_pass_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    ui_test_utils::NavigateToURL(
        browser(), GURL(fake_server_address_ + test_path_ + filename));
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
  string16 title_pass_;
  string16 title_storing_;
  string16 title_error_write_failed_;
  string16 title_error_empty_;
  std::string fake_server_address_;
  std::string test_path_;

  DISALLOW_COPY_AND_ASSIGN(BetterSessionRestoreTest);
};

// BetterSessionRestoreTest tests fail under AddressSanitizer, see
// http://crbug.com/156444.
#if defined(ADDRESS_SANITIZER)
# define MAYBE_PRE_SessionCookies DISABLED_PRE_SessionCookies
# define MAYBE_SessionCookies DISABLED_SessionCookies
# define MAYBE_PRE_SessionStorage DISABLED_PRE_SessionStorage
# define MAYBE_SessionStorage DISABLED_SessionStorage
#else
# define MAYBE_PRE_SessionCookies PRE_SessionCookies
# define MAYBE_SessionCookies SessionCookies
# define MAYBE_PRE_SessionStorage PRE_SessionStorage
# define MAYBE_SessionStorage SessionStorage
#endif

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, MAYBE_PRE_SessionCookies) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  StoreDataWithPage("session_cookies.html");
}

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, MAYBE_SessionCookies) {
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPage();
}

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, MAYBE_PRE_SessionStorage) {
  // Write the data on disk less lazily.
  dom_storage::DomStorageArea::DisableCommitDelayForTesting();
  SessionStartupPref::SetStartupPref(
      browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  StoreDataWithPage("session_storage.html");
}

// crbug.com/156981
IN_PROC_BROWSER_TEST_F(BetterSessionRestoreTest, MAYBE_SessionStorage) {
  CheckReloadedPage();
}
