// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_job.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

// We need to serve the test files so that PRE_Test and Test can access the same
// page using the same URL. In addition, perceived security origin of the page
// needs to stay the same, so e.g., redirecting the URL requests doesn't
// work. (If we used a test server, the PRE_Test and Test would have separate
// instances running on separate ports.)

base::LazyInstance<std::map<std::string, std::string> > g_file_contents =
    LAZY_INSTANCE_INITIALIZER;

net::URLRequestJob* URLRequestFaker(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  return new net::URLRequestTestJob(
      request, network_delegate, net::URLRequestTestJob::test_headers(),
      g_file_contents.Get()[request->url().path()], true);
}

base::LazyInstance<std::string> g_last_upload_bytes = LAZY_INSTANCE_INITIALIZER;

net::URLRequestJob* URLRequestFakerForPostRequests(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  // Read the uploaded data and store it to g_last_upload_bytes.
  const net::UploadDataStream* upload_data = request->get_upload();
  g_last_upload_bytes.Get().clear();
  if (upload_data) {
    const ScopedVector<net::UploadElementReader>& readers =
        upload_data->element_readers();
    for (size_t i = 0; i < readers.size(); ++i) {
      const net::UploadBytesElementReader* bytes_reader =
          readers[i]->AsBytesReader();
      if (bytes_reader) {
        g_last_upload_bytes.Get() +=
            std::string(bytes_reader->bytes(), bytes_reader->length());
      }
    }
  }
  return new net::URLRequestTestJob(
      request, network_delegate, net::URLRequestTestJob::test_headers(),
      "<html><head><title>PASS</title></head><body>Data posted</body></html>",
      true);
}

class FakeBackgroundModeManager : public BackgroundModeManager {
 public:
  FakeBackgroundModeManager()
      : BackgroundModeManager(
            CommandLine::ForCurrentProcess(),
            &g_browser_process->profile_manager()->GetProfileInfoCache()),
        background_mode_active_(false) {}

  void SetBackgroundModeActive(bool active) {
    background_mode_active_ = active;
  }

  virtual bool IsBackgroundModeActive() OVERRIDE {
    return background_mode_active_;
  }

 private:
  bool background_mode_active_;

};

}  // namespace

class BetterSessionRestoreTest : public InProcessBrowserTest {
 public:
  BetterSessionRestoreTest()
      : fake_server_address_("http://www.test.com/"),
        test_path_("session_restore/"),
        title_pass_(base::ASCIIToUTF16("PASS")),
        title_storing_(base::ASCIIToUTF16("STORING")),
        title_error_write_failed_(base::ASCIIToUTF16("ERROR_WRITE_FAILED")),
        title_error_empty_(base::ASCIIToUTF16("ERROR_EMPTY")) {
    // Set up the URL request filtering.
    std::vector<std::string> test_files;
    test_files.push_back("common.js");
    test_files.push_back("cookies.html");
    test_files.push_back("local_storage.html");
    test_files.push_back("post.html");
    test_files.push_back("post_with_password.html");
    test_files.push_back("session_cookies.html");
    test_files.push_back("session_storage.html");
    base::FilePath test_file_dir;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &test_file_dir));
    test_file_dir =
        test_file_dir.AppendASCII("chrome/test/data").AppendASCII(test_path_);

    for (std::vector<std::string>::const_iterator it = test_files.begin();
         it != test_files.end(); ++it) {
      base::FilePath path = test_file_dir.AppendASCII(*it);
      std::string contents;
      CHECK(base::ReadFileToString(path, &contents));
      g_file_contents.Get()["/" + test_path_ + *it] = contents;
      net::URLRequestFilter::GetInstance()->AddUrlHandler(
          GURL(fake_server_address_ + test_path_ + *it),
          &URLRequestFaker);
    }
    net::URLRequestFilter::GetInstance()->AddUrlHandler(
        GURL(fake_server_address_ + test_path_ + "posted.php"),
        &URLRequestFakerForPostRequests);
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfile(browser()->profile()));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();
    g_browser_process->set_background_mode_manager_for_test(
        scoped_ptr<BackgroundModeManager>(new FakeBackgroundModeManager));
  }

  void StoreDataWithPage(const std::string& filename) {
    StoreDataWithPage(browser(), filename);
  }

  void StoreDataWithPage(Browser* browser, const std::string& filename) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(web_contents, title_storing_);
    title_watcher.AlsoWaitForTitle(title_pass_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    ui_test_utils::NavigateToURL(
        browser, GURL(fake_server_address_ + test_path_ + filename));
    base::string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_storing_, final_title);
  }

  void NavigateAndCheckStoredData(const std::string& filename) {
    NavigateAndCheckStoredData(browser(), filename);
  }

  void NavigateAndCheckStoredData(Browser* browser,
                                  const std::string& filename) {
    // Navigate to a page which has previously stored data; check that the
    // stored data can be accessed.
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(web_contents, title_pass_);
    title_watcher.AlsoWaitForTitle(title_storing_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    ui_test_utils::NavigateToURL(
        browser, GURL(fake_server_address_ + test_path_ + filename));
    base::string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_pass_, final_title);
  }

  void CheckReloadedPageRestored() {
    CheckTitle(browser(), title_pass_);
  }

  void CheckReloadedPageRestored(Browser* browser) {
    CheckTitle(browser, title_pass_);
  }

  void CheckReloadedPageNotRestored() {
    CheckReloadedPageNotRestored(browser());
  }

  void CheckReloadedPageNotRestored(Browser* browser) {
    CheckTitle(browser, title_storing_);
  }

  void CheckTitle(Browser* browser, const base::string16& expected_title) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(title_pass_);
    title_watcher.AlsoWaitForTitle(title_storing_);
    title_watcher.AlsoWaitForTitle(title_error_write_failed_);
    title_watcher.AlsoWaitForTitle(title_error_empty_);
    // It's possible that the title was already the right one before
    // title_watcher was created.
    base::string16 first_title = web_contents->GetTitle();
    if (first_title != title_pass_ &&
        first_title != title_storing_ &&
        first_title != title_error_write_failed_ &&
        first_title != title_error_empty_) {
      base::string16 final_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, final_title);
    } else {
      EXPECT_EQ(expected_title, first_title);
    }
  }

  void PostFormWithPage(const std::string& filename, bool password_present) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(web_contents, title_pass_);
    ui_test_utils::NavigateToURL(
        browser(), GURL(fake_server_address_ + test_path_ + filename));
    base::string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(title_pass_, final_title);
    EXPECT_TRUE(g_last_upload_bytes.Get().find("posted-text") !=
                std::string::npos);
    EXPECT_TRUE(g_last_upload_bytes.Get().find("text-entered") !=
                std::string::npos);
    if (password_present) {
      EXPECT_TRUE(g_last_upload_bytes.Get().find("posted-password") !=
                  std::string::npos);
      EXPECT_TRUE(g_last_upload_bytes.Get().find("password-entered") !=
                  std::string::npos);
    }
  }

  void CheckFormRestored(bool text_present, bool password_present) {
    CheckFormRestored(browser(), text_present, password_present);
  }

  void CheckFormRestored(
      Browser* browser, bool text_present, bool password_present) {
    CheckReloadedPageRestored(browser);
    if (text_present) {
      EXPECT_TRUE(g_last_upload_bytes.Get().find("posted-text") !=
                  std::string::npos);
      EXPECT_TRUE(g_last_upload_bytes.Get().find("text-entered") !=
                  std::string::npos);
    } else {
      EXPECT_TRUE(g_last_upload_bytes.Get().find("posted-text") ==
                  std::string::npos);
      EXPECT_TRUE(g_last_upload_bytes.Get().find("text-entered") ==
                  std::string::npos);
    }
    if (password_present) {
      EXPECT_TRUE(g_last_upload_bytes.Get().find("posted-password") !=
                  std::string::npos);
      EXPECT_TRUE(g_last_upload_bytes.Get().find("password-entered") !=
                  std::string::npos);
    } else {
      EXPECT_TRUE(g_last_upload_bytes.Get().find("posted-password") ==
                  std::string::npos);
      EXPECT_TRUE(g_last_upload_bytes.Get().find("password-entered") ==
                  std::string::npos);
    }
  }

  void CloseBrowserSynchronously(Browser* browser, bool close_all_windows) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
    if (close_all_windows)
      chrome::CloseAllBrowsers();
    else
      browser->window()->Close();
#if defined(OS_MACOSX)
    // BrowserWindowController depends on the auto release pool being recycled
    // in the message loop to delete itself, which frees the Browser object
    // which fires this event.
    AutoreleasePool()->Recycle();
#endif
    observer.Wait();
  }

  virtual Browser* QuitBrowserAndRestore(Browser* browser,
                                         bool close_all_windows) {
    Profile* profile = browser->profile();

    // Close the browser.
    chrome::IncrementKeepAliveCount();
    CloseBrowserSynchronously(browser, close_all_windows);

    SessionServiceTestHelper helper;
    helper.SetService(
        SessionServiceFactory::GetForProfileForSessionRestore(profile));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();

    // Create a new window, which should trigger session restore.
    ui_test_utils::BrowserAddedObserver window_observer;
    chrome::NewEmptyWindow(profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
    Browser* new_browser = window_observer.WaitForSingleNewBrowser();
    chrome::DecrementKeepAliveCount();

    return new_browser;
  }

  std::string fake_server_address() {
    return fake_server_address_;
  }

  std::string test_path() {
    return test_path_;
  }

  void EnableBackgroundMode() {
    static_cast<FakeBackgroundModeManager*>(
        g_browser_process->background_mode_manager())->
        SetBackgroundModeActive(true);
  }

  void DisableBackgroundMode() {
    static_cast<FakeBackgroundModeManager*>(
        g_browser_process->background_mode_manager())->
        SetBackgroundModeActive(false);
  }

 private:
  const std::string fake_server_address_;
  const std::string test_path_;
  const base::string16 title_pass_;
  const base::string16 title_storing_;
  const base::string16 title_error_write_failed_;
  const base::string16 title_error_empty_;

  DISALLOW_COPY_AND_ASSIGN(BetterSessionRestoreTest);
};

class ContinueWhereILeftOffTest : public BetterSessionRestoreTest {
 public:
  ContinueWhereILeftOffTest() { }

  virtual void SetUpOnMainThread() OVERRIDE {
    BetterSessionRestoreTest::SetUpOnMainThread();
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::LAST));
  }

 protected:
  virtual Browser* QuitBrowserAndRestore(Browser* browser,
                                         bool close_all_windows) OVERRIDE {
    content::WindowedNotificationObserver session_restore_observer(
        chrome::NOTIFICATION_SESSION_RESTORE_DONE,
        content::NotificationService::AllSources());
    Browser* new_browser = BetterSessionRestoreTest::QuitBrowserAndRestore(
        browser, close_all_windows);
    session_restore_observer.Wait();
    return new_browser;
  }

  DISALLOW_COPY_AND_ASSIGN(ContinueWhereILeftOffTest);
};

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_SessionCookies) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, SessionCookies) {
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_SessionStorage) {
  StoreDataWithPage("session_storage.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, SessionStorage) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PRE_PRE_LocalStorageClearedOnExit) {
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PRE_LocalStorageClearedOnExit) {
  // Normally localStorage is restored.
  CheckReloadedPageRestored();
  // ... but not if it's set to clear on exit.
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, LocalStorageClearedOnExit) {
  CheckReloadedPageNotRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PRE_PRE_CookiesClearedOnExit) {
  StoreDataWithPage("cookies.html");
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_CookiesClearedOnExit) {
  // Normally cookies are restored.
  CheckReloadedPageRestored();
  // ... but not if the content setting is set to clear on exit.
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, CookiesClearedOnExit) {
  CheckReloadedPageNotRestored();
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_Post) {
  PostFormWithPage("post.html", false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, Post) {
  CheckFormRestored(true, false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PRE_PostWithPassword) {
  PostFormWithPage("post_with_password.html", true);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PostWithPassword) {
  CheckReloadedPageRestored();
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(false, false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, SessionCookiesBrowserClose) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored(new_browser);
}

// Test that leaving a popup open will not prevent session restore.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       SessionCookiesBrowserCloseWithPopupOpen) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
  Browser* popup = new Browser(Browser::CreateParams(
      Browser::TYPE_POPUP,
      browser()->profile(),
      chrome::HOST_DESKTOP_TYPE_NATIVE));
  popup->window()->Show();

  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored(new_browser);
}
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       CookiesClearedOnBrowserClose) {
  StoreDataWithPage("cookies.html");
  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  CheckReloadedPageRestored(new_browser);
  // ... but not if the content setting is set to clear on exit.
  CookieSettings::Factory::GetForProfile(new_browser->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  // ... unless background mode is active.
  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
  CheckReloadedPageRestored(new_browser);

  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    CheckReloadedPageRestored(new_browser);
  else
    CheckReloadedPageNotRestored(new_browser);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PostBrowserClose) {
  PostFormWithPage("post.html", false);
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  CheckFormRestored(new_browser, true, false);
}

IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PostWithPasswordBrowserClose) {
  PostFormWithPage("post_with_password.html", true);
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  CheckReloadedPageRestored(new_browser);
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(new_browser, false, false);
}

// Check that session cookies are cleared on a wrench menu quit.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       SessionCookiesCloseAllBrowsers) {
  // Set the startup preference to "continue where I left off" and visit a page
  // which stores a session cookie.
  StoreDataWithPage("session_cookies.html");
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  // The browsing session will be continued; just wait for the page to reload
  // and check the stored data.
  CheckReloadedPageRestored(new_browser);
}

// Check that cookies are cleared on a wrench menu quit only if cookies are set
// to current session only, regardless of whether background mode is enabled.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       CookiesClearedOnCloseAllBrowsers) {
  StoreDataWithPage("cookies.html");
  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  CheckReloadedPageRestored(new_browser);
  // ... but not if the content setting is set to clear on exit.
  CookieSettings::Factory::GetForProfile(new_browser->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  // ... even if background mode is active.
  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  CheckReloadedPageNotRestored(new_browser);

  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  CheckReloadedPageNotRestored(new_browser);
}

// Check that form data is restored after wrench menu quit.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest, PostCloseAllBrowsers) {
  PostFormWithPage("post.html", false);
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  CheckFormRestored(new_browser, true, false);
}

// Check that form data with a password field is cleared after wrench menu quit.
IN_PROC_BROWSER_TEST_F(ContinueWhereILeftOffTest,
                       PostWithPasswordCloseAllBrowsers) {
  PostFormWithPage("post_with_password.html", true);
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  CheckReloadedPageRestored(new_browser);
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(new_browser, false, false);
}

class RestartTest : public BetterSessionRestoreTest {
 public:
  RestartTest() { }
  virtual ~RestartTest() { }
 protected:
  void Restart() {
    // Simluate restarting the browser, but let the test exit peacefully.
    for (chrome::BrowserIterator it; !it.done(); it.Next())
      content::BrowserContext::SaveSessionState(it->profile());
    PrefService* pref_service = g_browser_process->local_state();
    pref_service->SetBoolean(prefs::kWasRestarted, true);
#if defined(OS_WIN)
    if (pref_service->HasPrefPath(prefs::kRelaunchMode))
      pref_service->ClearPref(prefs::kRelaunchMode);
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RestartTest);
};

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_SessionCookies) {
  StoreDataWithPage("session_cookies.html");
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, SessionCookies) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_SessionStorage) {
  StoreDataWithPage("session_storage.html");
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, SessionStorage) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_LocalStorageClearedOnExit) {
  StoreDataWithPage("local_storage.html");
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, LocalStorageClearedOnExit) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_CookiesClearedOnExit) {
  StoreDataWithPage("cookies.html");
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, CookiesClearedOnExit) {
  CheckReloadedPageRestored();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_Post) {
  PostFormWithPage("post.html", false);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, Post) {
  CheckFormRestored(true, false);
}

IN_PROC_BROWSER_TEST_F(RestartTest, PRE_PostWithPassword) {
  PostFormWithPage("post_with_password.html", true);
  Restart();
}

IN_PROC_BROWSER_TEST_F(RestartTest, PostWithPassword) {
  // The form data contained passwords, so it's removed completely.
  CheckFormRestored(false, false);
}

// These tests ensure that the Better Session Restore features are not triggered
// when they shouldn't be.
class NoSessionRestoreTest : public BetterSessionRestoreTest {
 public:
  NoSessionRestoreTest() { }

  virtual void SetUpOnMainThread() OVERRIDE {
    BetterSessionRestoreTest::SetUpOnMainThread();
    SessionStartupPref::SetStartupPref(
        browser()->profile(), SessionStartupPref(SessionStartupPref::DEFAULT));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoSessionRestoreTest);
};

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_SessionCookies) {
  StoreDataWithPage("session_cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionCookies) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  // When we navigate to the page again, it doens't see the data previously
  // stored.
  StoreDataWithPage("session_cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_SessionStorage) {
  StoreDataWithPage("session_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionStorage) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  StoreDataWithPage("session_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       PRE_PRE_LocalStorageClearedOnExit) {
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_LocalStorageClearedOnExit) {
  // Normally localStorage is persisted.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  NavigateAndCheckStoredData("local_storage.html");
  // ... but not if it's set to clear on exit.
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, LocalStorageClearedOnExit) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_PRE_CookiesClearedOnExit) {
  StoreDataWithPage("cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, PRE_CookiesClearedOnExit) {
  // Normally cookies are restored.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  NavigateAndCheckStoredData("cookies.html");
  // ... but not if the content setting is set to clear on exit.
  CookieSettings::Factory::GetForProfile(browser()->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnExit) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(std::string(url::kAboutBlankURL), web_contents->GetURL().spec());
  StoreDataWithPage("local_storage.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionCookiesBrowserClose) {
  StoreDataWithPage("session_cookies.html");
  EnableBackgroundMode();
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  NavigateAndCheckStoredData(new_browser, "session_cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    NavigateAndCheckStoredData(new_browser, "session_cookies.html");
  else
    StoreDataWithPage(new_browser, "session_cookies.html");
}

// Tests that session cookies are not cleared when only a popup window is open.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       SessionCookiesBrowserCloseWithPopupOpen) {
  StoreDataWithPage("session_cookies.html");
  Browser* popup = new Browser(Browser::CreateParams(
      Browser::TYPE_POPUP,
      browser()->profile(),
      chrome::HOST_DESKTOP_TYPE_NATIVE));
  popup->window()->Show();
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  NavigateAndCheckStoredData(new_browser, "session_cookies.html");
}

// Tests that session cookies are cleared if the last window to close is a
// popup.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest,
                       SessionCookiesBrowserClosePopupLast) {
  StoreDataWithPage("session_cookies.html");
  Browser* popup = new Browser(Browser::CreateParams(
      Browser::TYPE_POPUP,
      browser()->profile(),
      chrome::HOST_DESKTOP_TYPE_NATIVE));
  popup->window()->Show();
  CloseBrowserSynchronously(browser(), false);
  Browser* new_browser = QuitBrowserAndRestore(popup, false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    NavigateAndCheckStoredData(new_browser, "session_cookies.html");
  else
    StoreDataWithPage(new_browser, "session_cookies.html");
}

IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnBrowserClose) {
  StoreDataWithPage("cookies.html");

  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), false);
  NavigateAndCheckStoredData(new_browser, "cookies.html");

  // ... but not if the content setting is set to clear on exit.
  CookieSettings::Factory::GetForProfile(new_browser->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  // ... unless background mode is active.
  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
  NavigateAndCheckStoredData(new_browser, "cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, false);
  if (browser_defaults::kBrowserAliveWithNoWindows)
    NavigateAndCheckStoredData(new_browser, "cookies.html");
  else
    StoreDataWithPage(new_browser, "cookies.html");
}

// Check that session cookies are cleared on a wrench menu quit.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, SessionCookiesCloseAllBrowsers) {
  StoreDataWithPage("session_cookies.html");
  EnableBackgroundMode();
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  StoreDataWithPage(new_browser, "session_cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "session_cookies.html");
}

// Check that cookies are cleared on a wrench menu quit only if cookies are set
// to current session only, regardless of whether background mode is enabled.
IN_PROC_BROWSER_TEST_F(NoSessionRestoreTest, CookiesClearedOnCloseAllBrowsers) {
  StoreDataWithPage("cookies.html");

  // Normally cookies are restored.
  Browser* new_browser = QuitBrowserAndRestore(browser(), true);
  NavigateAndCheckStoredData(new_browser, "cookies.html");

  // ... but not if the content setting is set to clear on exit.
  CookieSettings::Factory::GetForProfile(new_browser->profile())->
      SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  // ... even if background mode is active.
  EnableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "cookies.html");
  DisableBackgroundMode();
  new_browser = QuitBrowserAndRestore(new_browser, true);
  StoreDataWithPage(new_browser, "cookies.html");
}
