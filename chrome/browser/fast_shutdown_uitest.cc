// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/stl_util.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/test/test_browser_thread.h"

using content::BrowserThread;

class FastShutdown : public UITest {
 protected:
  FastShutdown()
      : db_thread_(BrowserThread::DB),
        io_thread_(BrowserThread::IO),
        thread_helper_(new base::ThreadTestHelper(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB))) {
    dom_automation_enabled_ = true;
  }

  void Init() {
    ASSERT_TRUE(db_thread_.Start());
    ASSERT_TRUE(io_thread_.Start());
    // Cache this, so that we can still access it after the browser exits.
    user_data_dir_ = user_data_dir();
  }

  // Looks for the given |cookie| in the cookie store. If it exists, puts the
  // cookie's value in |cookie_value| and sets |has_cookie| to true. Sets
  // |has_cookie| to false if the |cookie| wasn't found.
  void GetCookie(const net::CookieMonster::CanonicalCookie& cookie,
                 bool* has_cookie, std::string* cookie_value) {
    scoped_refptr<SQLitePersistentCookieStore> cookie_store(
       new SQLitePersistentCookieStore(
           user_data_dir_.AppendASCII(chrome::kInitialProfile)
                         .Append(chrome::kCookieFilename),
           false));
    std::vector<net::CookieMonster::CanonicalCookie*> cookies;
    cookie_store->Load(
        base::Bind(&FastShutdown::LoadCookiesCallback,
                   base::Unretained(this),
                   MessageLoop::current(),
                   base::Unretained(&cookies)));
    // Will receive a QuitTask when LoadCookiesCallback is invoked.
    MessageLoop::current()->Run();
    *has_cookie = false;
    for (size_t i = 0; i < cookies.size(); ++i) {
      if (cookies[i]->IsEquivalent(cookie)) {
        *has_cookie = true;
        *cookie_value = cookies[i]->Value();
      }
    }
    cookie_store = NULL;
    ASSERT_TRUE(thread_helper_->Run());
    STLDeleteElements(&cookies);
  }

 private:
  void LoadCookiesCallback(
      MessageLoop* to_notify,
      std::vector<net::CookieMonster::CanonicalCookie*>* cookies,
      const std::vector<net::CookieMonster::CanonicalCookie*>& cookies_get) {
    *cookies = cookies_get;
    to_notify->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  content::TestBrowserThread db_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<base::ThreadTestHelper> thread_helper_;
  FilePath user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(FastShutdown);
};

// This tests for a previous error where uninstalling an onbeforeunload handler
// would enable fast shutdown even if an onunload handler still existed.
// Flaky on all platforms, http://crbug.com/89173
TEST_F(FastShutdown, FLAKY_SlowTermination) {
  Init();

  // Only the name, domain and path are used in IsEquivalent(), so we don't care
  // what the other fields have.
  net::CookieMonster::CanonicalCookie cookie(
      GURL(),        // url
      "unloaded",    // name
      "",            // value
      "",            // domain
      "/",           // path
      "",            // mac_key
      "",            // mac_algorithm
      base::Time(),  // creation
      base::Time(),  // expiration
      base::Time(),  // last_access
      false,         // secure
      false,         // httponly
      false,         // has_expires
      false);        // is_persistent

  // This page has an unload handler.
  const FilePath dir(FILE_PATH_LITERAL("fast_shutdown"));
  const FilePath file(FILE_PATH_LITERAL("on_unloader.html"));

  NavigateToURL(ui_test_utils::GetTestUrl(dir, file));

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));

  // This page has a beforeunload handler.
  ASSERT_TRUE(browser->GetTab(0)->ExecuteJavaScript(
      "open('on_before_unloader.html')"));
  WaitUntilTabCount(2);

  // Close the tab, removing the one and only beforeunload handler.
  ASSERT_TRUE(browser->GetTab(1)->Close(true));

  // Close the browser. This should launch the unload handler, which sets a
  // cookie that's stored to disk.
  QuitBrowser();

  bool has_cookie = false;
  std::string cookie_value;
  // Read the cookie and check that it has the expected value.
  GetCookie(cookie, &has_cookie, &cookie_value);
  EXPECT_TRUE(has_cookie);
  EXPECT_EQ("ohyeah", cookie_value);
}
