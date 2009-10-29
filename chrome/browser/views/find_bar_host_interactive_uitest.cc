// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/keyboard_codes.h"
#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/find_bar_host.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/view_ids.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/url_request/url_request_unittest.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"

namespace {

// The delay waited after sending an OS simulated event.
static const int kActionDelayMs = 500;
static const wchar_t kDocRoot[] = L"chrome/test/data";
static const wchar_t kSimplePage[] = L"404_is_enough_for_us.html";

class FindInPageTest : public InProcessBrowserTest {
 public:
  FindInPageTest() {
    set_show_window(true);
    FindBarHost::disable_animations_during_testing_ = true;
  }

  void ClickOnView(ViewID view_id) {
    BrowserWindow* browser_window = browser()->window();
    ASSERT_TRUE(browser_window);
#if defined(TOOLKIT_VIEWS)
    views::View* view =
        reinterpret_cast<BrowserView*>(browser_window)->GetViewByID(view_id);
#elif defined(OS_LINUX)
    gfx::NativeWindow window = browser_window->GetNativeHandle();
    ASSERT_TRUE(window);
    GtkWidget* view = ViewIDUtil::GetWidget(GTK_WIDGET(window), view_id);
#endif
    ASSERT_TRUE(view);
    ui_controls::MoveMouseToCenterAndPress(view,
                                           ui_controls::LEFT,
                                           ui_controls::DOWN | ui_controls::UP,
                                           new MessageLoop::QuitTask());
    ui_test_utils::RunMessageLoop();
  }

  int GetFocusedViewID() {
#if defined(TOOLKIT_VIEWS)
    views::FocusManager* focus_manager =
        views::FocusManager::GetFocusManagerForNativeView(
            browser()->window()->GetNativeHandle());
    if (!focus_manager) {
      NOTREACHED();
      return -1;
    }
    views::View* focused_view = focus_manager->GetFocusedView();
    if (!focused_view)
      return -1;
    return focused_view->GetID();
#else
    return -1;
#endif
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FindInPageTest, CrashEscHandlers) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  // First we navigate to our test page (tab A).
  GURL url = server->TestServerPageW(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  browser()->Find();

  // Open another tab (tab B).
  browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED, true, -1,
                           false, NULL);

  browser()->Find();
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, GetFocusedViewID());

  // Select tab A.
  browser()->SelectTabContentsAt(0, true);

  // Close tab B.
  browser()->CloseTabContents(browser()->GetTabContentsAt(1));

  // Click on the location bar so that Find box loses focus.
  ClickOnView(VIEW_ID_LOCATION_BAR);
#if defined(TOOLKIT_VIEWS) || defined(OS_WIN)
  // Check the location bar is focused.
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, GetFocusedViewID());
#endif

  // This used to crash until bug 1303709 was fixed.
  ui_controls::SendKeyPressNotifyWhenDone(
      browser()->window()->GetNativeHandle(), base::VKEY_ESCAPE,
      true, false, false, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
}

// TODO: http://crbug.com/26231
IN_PROC_BROWSER_TEST_F(FindInPageTest, FocusRestore) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  GURL url = server->TestServerPageW(L"title1.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Focus the location bar, open and close the find-in-page, focus should
  // return to the location bar.
  browser()->FocusLocationBar();
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, GetFocusedViewID());
  // Ensure the creation of the find bar controller.
  browser()->GetFindBarController()->Show();
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, GetFocusedViewID());
  browser()->GetFindBarController()->EndFindSession();
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, GetFocusedViewID());

  // Focus the location bar, find something on the page, close the find box,
  // focus should go to the page.
  browser()->FocusLocationBar();
  browser()->Find();
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, GetFocusedViewID());
  ui_test_utils::FindInPage(browser()->GetSelectedTabContents(),
                            L"a", true, false, NULL);
  browser()->GetFindBarController()->EndFindSession();
  EXPECT_EQ(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW, GetFocusedViewID());

  // Focus the location bar, open and close the find box, focus should return to
  // the location bar (same as before, just checking that http://crbug.com/23599
  // is fixed).
  browser()->FocusLocationBar();
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, GetFocusedViewID());
  browser()->GetFindBarController()->Show();
  EXPECT_EQ(VIEW_ID_FIND_IN_PAGE_TEXT_FIELD, GetFocusedViewID());
  browser()->GetFindBarController()->EndFindSession();
  EXPECT_EQ(VIEW_ID_LOCATION_BAR, GetFocusedViewID());
}
