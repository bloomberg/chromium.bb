// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/stl_util.h"
#include "base/test/test_timeouts.h"
#include "base/test/thread_test_helper.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "ui/base/events.h"
#include "ui/base/message_box_flags.h"
#include "ui/gfx/rect.h"

class FastShutdown : public UITest {
};

#if defined(OS_MACOSX)
// SimulateOSClick is broken on the Mac: http://crbug.com/45162
#define MAYBE_SlowTermination DISABLED_SlowTermination
#elif defined(OS_CHROMEOS)
// Flaky on chromeos: http://crbug.com/89173
#define MAYBE_SlowTermination FLAKY_SlowTermination
#else
#define MAYBE_SlowTermination SlowTermination
#endif

// Loads the cookie store from |user_data_dir|. If the given |cookie| exists,
// puts the cookie's value in |cookie_value| and sets |has_cookie| to true.
// Sets |has_cookie| to false if the |cookie| wasn't found.
static void GetCookie(
    const FilePath& user_data_dir,
    const net::CookieMonster::CanonicalCookie& cookie,
    bool* has_cookie,
    std::string* cookie_value,
    const scoped_refptr<base::ThreadTestHelper>& thread_helper) {
  scoped_refptr<SQLitePersistentCookieStore> cookie_store(
      new SQLitePersistentCookieStore(
          user_data_dir.AppendASCII(chrome::kNotSignedInProfile)
                       .Append(chrome::kCookieFilename)));
  std::vector<net::CookieMonster::CanonicalCookie*> cookies;
  ASSERT_TRUE(cookie_store->Load(&cookies));
  *has_cookie = false;
  for (size_t i = 0; i < cookies.size(); ++i) {
    if (cookies[i]->IsEquivalent(cookie)) {
      *has_cookie = true;
      *cookie_value = cookies[i]->Value();
    }
  }
  cookie_store = NULL;
  ASSERT_TRUE(thread_helper->Run());
  STLDeleteElements(&cookies);
}

// This tests for a previous error where uninstalling an onbeforeunload
// handler would enable fast shutdown even if an onUnload handler still
// existed.
TEST_F(FastShutdown, MAYBE_SlowTermination) {
  // Set up the DB thread (used by the cookie store during its clean up).
  BrowserThread db_thread(BrowserThread::DB);
  db_thread.Start();
  scoped_refptr<base::ThreadTestHelper> thread_helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));

  // Only the name, domain and path are used in IsEquivalent(), so we don't
  // care what the other fields have.
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
      false);        // has_expires

  bool has_cookie = false;
  std::string cookie_value;

  // Check that the cookie (to be set during unload) doesn't exist already.
  GetCookie(user_data_dir(), cookie, &has_cookie, &cookie_value, thread_helper);
  EXPECT_FALSE(has_cookie);
  EXPECT_EQ("", cookie_value);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // This page has an unload handler.
  const FilePath dir(FILE_PATH_LITERAL("fast_shutdown"));
  const FilePath file(FILE_PATH_LITERAL("on_unloader.html"));
  NavigateToURL(ui_test_utils::GetTestUrl(dir, file));
  gfx::Rect bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &bounds, true));
  // This click will launch a popup which has a before unload handler.
  ASSERT_TRUE(window->SimulateOSClick(bounds.CenterPoint(),
                                      ui::EF_LEFT_BUTTON_DOWN));
  ASSERT_TRUE(browser->WaitForTabCountToBecome(2));
  // Close the tab, removing the one and only before unload handler.
  scoped_refptr<TabProxy> tab(browser->GetTab(1));
  ASSERT_TRUE(tab->Close(true));

  // Close the browser. We should launch the unload handler, which sets a
  // cookie that's stored to disk.
  ASSERT_TRUE(browser->ApplyAccelerator(IDC_CLOSE_WINDOW));

  int exit_code = -1;
  ASSERT_TRUE(launcher_->WaitForBrowserProcessToQuit(
                  TestTimeouts::action_max_timeout_ms(), &exit_code));
  EXPECT_EQ(0, exit_code);  // Expect a clean shutdown.

  // Read the cookie and check that it has the expected value.
  GetCookie(user_data_dir(), cookie, &has_cookie, &cookie_value, thread_helper);
  EXPECT_TRUE(has_cookie);
  EXPECT_EQ("ohyeah", cookie_value);
}
