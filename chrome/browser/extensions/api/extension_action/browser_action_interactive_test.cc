// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/base/ui_features.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {
namespace {

// Helper to ensure all extension hosts are destroyed during the test. If a host
// is still alive, the Profile can not be destroyed in
// BrowserProcessImpl::StartTearDown(). TODO(tapted): The existence of this
// helper is probably a bug. Extension hosts do not currently block shutdown the
// way a browser tab does. Maybe they should. See http://crbug.com/729476.
class ExtensionHostWatcher : public content::NotificationObserver {
 public:
  ExtensionHostWatcher() {
    registrar_.Add(this, NOTIFICATION_EXTENSION_HOST_CREATED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, NOTIFICATION_EXTENSION_HOST_DESTROYED,
                   content::NotificationService::AllSources());
  }

  void Wait() {
    if (created_ == destroyed_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();
  }

  int created() const { return created_; }
  int destroyed() const { return destroyed_; }

  // NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ++(type == NOTIFICATION_EXTENSION_HOST_CREATED ? created_ : destroyed_);
    if (!quit_closure_.is_null() && created_ == destroyed_)
      quit_closure_.Run();
  }

 private:
  content::NotificationRegistrar registrar_;
  base::Closure quit_closure_;
  int created_ = 0;
  int destroyed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHostWatcher);
};

// chrome.browserAction API tests that interact with the UI in such a way that
// they cannot be run concurrently (i.e. openPopup API tests that require the
// window be focused/active).
class BrowserActionInteractiveTest : public ExtensionApiTest {
 public:
  BrowserActionInteractiveTest() {}
  ~BrowserActionInteractiveTest() override {}

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    host_watcher_ = base::MakeUnique<ExtensionHostWatcher>();
    ExtensionApiTest::SetUpOnMainThread();
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  void TearDownOnMainThread() override {
    // Note browser windows are closed in PostRunTestOnMainThread(), which is
    // called after this. But relying on the window close to close the
    // extension host can cause flakes. See http://crbug.com/729476.
    // Waiting here requires individual tests to ensure their popup has closed.
    ExtensionApiTest::TearDownOnMainThread();
    host_watcher_->Wait();
    EXPECT_EQ(host_watcher_->created(), host_watcher_->destroyed());
  }

 protected:
  // Function to control whether to run popup tests for the current platform.
  // These tests require RunExtensionSubtest to work as expected and the browser
  // window to able to be made active automatically. Returns false for platforms
  // where these conditions are not met.
  bool ShouldRunPopupTest() {
    // TODO(justinlin): http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
    return false;
#else
    return true;
#endif
  }

  void EnsurePopupActive() {
    BrowserActionTestUtil test_util(browser());
    EXPECT_TRUE(test_util.HasPopup());
    EXPECT_TRUE(test_util.WaitForPopup());
    EXPECT_TRUE(test_util.HasPopup());
  }

  // Open an extension popup via the chrome.browserAction.openPopup API.
  void OpenPopupViaAPI() {
    // Setup the notification observer to wait for the popup to finish loading.
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    // Show first popup in first window and expect it to have loaded.
    ASSERT_TRUE(RunExtensionSubtest("browser_action/open_popup",
                                    "open_popup_succeeds.html")) << message_;
    frame_observer.Wait();
    EnsurePopupActive();
  }

  // Open an extension popup by clicking the browser action button.
  void OpenPopupViaToolbar() {
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    BrowserActionTestUtil(browser()).Press(0);
    frame_observer.Wait();
    EnsurePopupActive();
  }

  // Close the popup window directly.
  void ClosePopup() { BrowserActionTestUtil(browser()).HidePopup(); }

  // Trigger a focus loss to close the popup.
  void ClosePopupViaFocusLoss() {
    EXPECT_TRUE(BrowserActionTestUtil(browser()).HasPopup());
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());

#if defined(OS_MACOSX)
    // ClickOnView() in an inactive window is not robust on Mac. The click does
    // not guarantee window activation on trybots. So activate the browser
    // explicitly, thus causing the bubble to lose focus and dismiss itself.
    // This works because bubbles on Mac are always toplevel.
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#else
    // Elsewhere, click on the omnibox. Note that with aura, the browser may be
    // "active" the entire time when the popup is not a toplevel window. It's
    // aura::Window::Focus() that determines where key events go in this case.
    ui_test_utils::ClickOnView(browser(), VIEW_ID_OMNIBOX);
#endif

    // The window disappears immediately.
    EXPECT_FALSE(BrowserActionTestUtil(browser()).HasPopup());

    // Wait for the notification to achieve a consistent state and verify that
    // the popup was properly torn down.
    observer.Wait();
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::unique_ptr<ExtensionHostWatcher> host_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionInteractiveTest);
};

// Tests opening a popup using the chrome.browserAction.openPopup API. This test
// opens a popup in the starting window, closes the popup, creates a new window
// and opens a popup in the new window. Both popups should succeed in opening.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TestOpenPopup) {
  if (!ShouldRunPopupTest())
    return;

  BrowserActionTestUtil browserActionBar(browser());
  // Setup extension message listener to wait for javascript to finish running.
  ExtensionTestMessageListener listener("ready", true);
  {
    OpenPopupViaAPI();
    EXPECT_TRUE(browserActionBar.HasPopup());
    browserActionBar.HidePopup();
  }

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  Browser* new_browser = NULL;
  {
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    // Open a new window.
    new_browser = chrome::FindBrowserWithWebContents(browser()->OpenURL(
        content::OpenURLParams(GURL("about:"), content::Referrer(),
                               WindowOpenDisposition::NEW_WINDOW,
                               ui::PAGE_TRANSITION_TYPED, false)));
    // Hide all the buttons to test that it opens even when the browser action
    // is in the overflow bucket.
    ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);
    frame_observer.Wait();
  }

  EXPECT_TRUE(new_browser != NULL);

  ResultCatcher catcher;
  {
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    // Show second popup in new window.
    listener.Reply("show another");
    frame_observer.Wait();
    EXPECT_TRUE(BrowserActionTestUtil(new_browser).HasPopup());
  }
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
  BrowserActionTestUtil(new_browser).HidePopup();
}

// Tests opening a popup in an incognito window.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TestOpenPopupIncognito) {
  if (!ShouldRunPopupTest())
    return;

  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  ASSERT_TRUE(RunExtensionSubtest("browser_action/open_popup",
                                  "open_popup_succeeds.html",
                                  kFlagEnableIncognito | kFlagUseIncognito))
      << message_;
  frame_observer.Wait();
  // Non-Aura Linux uses a singleton for the popup, so it looks like all windows
  // have popups if there is any popup open.
#if !(defined(OS_LINUX) && !defined(USE_AURA))
  // Starting window does not have a popup.
  EXPECT_FALSE(BrowserActionTestUtil(browser()).HasPopup());
#endif
  // Incognito window should have a popup.
  BrowserActionTestUtil test_util(BrowserList::GetInstance()->GetLastActive());
  EXPECT_TRUE(test_util.HasPopup());
  test_util.HidePopup();
}

// Tests that an extension can open a popup in the last active incognito window
// even from a background page with a non-incognito profile.
// (crbug.com/448853)
#if defined(OS_WIN)
// Fails on XP: http://crbug.com/515717
#define MAYBE_TestOpenPopupIncognitoFromBackground \
  DISABLED_TestOpenPopupIncognitoFromBackground
#else
#define MAYBE_TestOpenPopupIncognitoFromBackground \
  TestOpenPopupIncognitoFromBackground
#endif
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       MAYBE_TestOpenPopupIncognitoFromBackground) {
  if (!ShouldRunPopupTest())
    return;

  const Extension* extension =
      LoadExtensionIncognito(test_data_dir_.AppendASCII("browser_action").
          AppendASCII("open_popup_background"));
  ASSERT_TRUE(extension);
  ExtensionTestMessageListener listener(false);
  listener.set_extension_id(extension->id());

  Browser* incognito_browser =
      OpenURLOffTheRecord(profile(), GURL("chrome://newtab/"));
  listener.WaitUntilSatisfied();
  EXPECT_EQ(std::string("opened"), listener.message());
  BrowserActionTestUtil test_util(incognito_browser);
  EXPECT_TRUE(test_util.HasPopup());
  test_util.HidePopup();
}

#if defined(OS_LINUX)
#define MAYBE_TestOpenPopupDoesNotCloseOtherPopups DISABLED_TestOpenPopupDoesNotCloseOtherPopups
#else
#define MAYBE_TestOpenPopupDoesNotCloseOtherPopups TestOpenPopupDoesNotCloseOtherPopups
#endif
// Tests if there is already a popup open (by a user click or otherwise), that
// the openPopup API does not override it.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       MAYBE_TestOpenPopupDoesNotCloseOtherPopups) {
  if (!ShouldRunPopupTest())
    return;

  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  ExtensionTestMessageListener listener("ready", true);
  // Load the test extension which will do nothing except notifyPass() to
  // return control here.
  ASSERT_TRUE(RunExtensionSubtest("browser_action/open_popup",
                                  "open_popup_fails.html")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  OpenPopupViaToolbar();
  ResultCatcher catcher;
  // Return control to javascript to validate that opening a popup fails now.
  listener.Reply("show another");
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
  ClosePopup();
}

// Test that openPopup does not grant tab permissions like for browser action
// clicks if the activeTab permission is set.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupDoesNotGrantTabPermissions) {
  if (!ShouldRunPopupTest())
    return;

  OpenPopupViaAPI();
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  ASSERT_FALSE(
      service->GetExtensionById(last_loaded_extension_id(), false)
          ->permissions_data()
          ->HasAPIPermissionForTab(
              SessionTabHelper::IdForTab(
                  browser()->tab_strip_model()->GetActiveWebContents()),
              APIPermission::kTab));
  ClosePopup();
}

// Test that the extension popup is closed when the browser window is focused.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, FocusLossClosesPopup1) {
  if (!ShouldRunPopupTest())
    return;
  OpenPopupViaAPI();
  ClosePopupViaFocusLoss();
}

// Test that the extension popup is closed when the browser window is focused.
#if defined(OS_WIN)
// Flaky on Windows: http://crbug.com/639130
#define MAYBE_FocusLossClosesPopup2 DISABLED_FocusLossClosesPopup2
#else
#define MAYBE_FocusLossClosesPopup2 FocusLossClosesPopup2
#endif
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       MAYBE_FocusLossClosesPopup2) {
  if (!ShouldRunPopupTest())
    return;

  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;
  OpenPopupViaToolbar();
  ClosePopupViaFocusLoss();
}

// Test that the extension popup is closed on browser tab switches.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TabSwitchClosesPopup) {
  if (!ShouldRunPopupTest())
    return;

  // Add a second tab to the browser and open an extension popup.
  chrome::NewTab(browser());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1),
            browser()->tab_strip_model()->GetActiveWebContents());
  OpenPopupViaAPI();

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  // Change active tabs, the extension popup should close.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  observer.Wait();

  EXPECT_FALSE(BrowserActionTestUtil(browser()).HasPopup());
}

IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       DeleteBrowserActionWithPopupOpen) {
  if (!ShouldRunPopupTest())
    return;

  // First, we open a popup.
  OpenPopupViaAPI();
  BrowserActionTestUtil browser_action_test_util(browser());
  EXPECT_TRUE(browser_action_test_util.HasPopup());

  // Then, find the extension that created it.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  GURL url = active_web_contents->GetLastCommittedURL();
  const Extension* extension = ExtensionRegistry::Get(browser()->profile())->
      enabled_extensions().GetExtensionOrAppByURL(url);
  ASSERT_TRUE(extension);

  // Finally, uninstall the extension, which causes the view to be deleted and
  // the popup to go away. This should not crash.
  UninstallExtension(extension->id());
  EXPECT_FALSE(browser_action_test_util.HasPopup());
}

// BrowserActionTestUtil::InspectPopup() is not implemented for a Cocoa browser.
#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#define MAYBE_CloseBrowserWithDevTools CloseBrowserWithDevTools
#else
#define MAYBE_CloseBrowserWithDevTools DISABLED_CloseBrowserWithDevTools
#endif
// Test closing the browser while inspecting an extension popup with dev tools.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       MAYBE_CloseBrowserWithDevTools) {
  if (!ShouldRunPopupTest())
    return;

  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Open an extension popup by clicking the browser action button.
  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  BrowserActionTestUtil(browser()).InspectPopup(0);
  frame_observer.Wait();
  EXPECT_TRUE(BrowserActionTestUtil(browser()).HasPopup());

  // Close the browser window, this should not cause a crash.
  chrome::CloseWindow(browser());
}

#if defined(OS_WIN)
// Test that forcibly closing the browser and popup HWND does not cause a crash.
// http://crbug.com/400646
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       DISABLED_DestroyHWNDDoesNotCrash) {
  if (!ShouldRunPopupTest())
    return;

  OpenPopupViaAPI();
  BrowserActionTestUtil test_util(browser());
  const gfx::NativeView view = test_util.GetPopupNativeView();
  EXPECT_NE(static_cast<gfx::NativeView>(NULL), view);
  const HWND hwnd = views::HWNDForNativeView(view);
  EXPECT_EQ(hwnd,
            views::HWNDForNativeView(browser()->window()->GetNativeWindow()));
  EXPECT_EQ(TRUE, ::IsWindow(hwnd));

  // Create a new browser window to prevent the message loop from terminating.
  browser()->OpenURL(content::OpenURLParams(GURL("about:"), content::Referrer(),
                                            WindowOpenDisposition::NEW_WINDOW,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Forcibly closing the browser HWND should not cause a crash.
  EXPECT_EQ(TRUE, ::CloseWindow(hwnd));
  EXPECT_EQ(TRUE, ::DestroyWindow(hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(hwnd));
}
#endif  // OS_WIN

}  // namespace
}  // namespace extensions
