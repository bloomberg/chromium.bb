// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "base/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/atl_module.h"
#include "ui/views/accessibility/native_view_accessibility_win.h"
#include "ui/views/widget/widget.h"

namespace {

VARIANT id_self = {VT_I4, CHILDID_SELF};

}  // namespace

class BrowserViewsAccessibilityTest : public InProcessBrowserTest {
 public:
  BrowserViewsAccessibilityTest();
  virtual ~BrowserViewsAccessibilityTest();

  // Retrieves an instance of BrowserWindowTesting.
  BrowserWindowTesting* GetBrowserWindowTesting();

  // Retrieve an instance of BrowserView.
  BrowserView* GetBrowserView();

  // Retrieves and initializes an instance of ToolbarView.
  ToolbarView* GetToolbarView();

  // Retrieves and initializes an instance of BookmarkBarView.
  BookmarkBarView* GetBookmarkBarView();

  // Retrieves and verifies the accessibility object for the given View.
  void TestViewAccessibilityObject(views::View* view,
                                   std::wstring name,
                                   int32 role);

  // Verifies MSAA Name and Role properties of the given IAccessible.
  void TestAccessibilityInfo(IAccessible* acc_obj,
                             std::wstring name,
                             int32 role);

 private:
  base::win::ScopedCOMInitializer com_initializer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewsAccessibilityTest);
};

BrowserViewsAccessibilityTest::BrowserViewsAccessibilityTest() {
  ui::win::CreateATLModuleIfNeeded();
}

BrowserViewsAccessibilityTest::~BrowserViewsAccessibilityTest() {
}

BrowserWindowTesting* BrowserViewsAccessibilityTest::GetBrowserWindowTesting() {
  BrowserWindow* browser_window = browser()->window();
  return browser_window ? browser_window->GetBrowserWindowTesting() : NULL;
}

BrowserView* BrowserViewsAccessibilityTest::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(browser());
}

ToolbarView* BrowserViewsAccessibilityTest::GetToolbarView() {
  BrowserWindowTesting* browser_window_testing = GetBrowserWindowTesting();
  return browser_window_testing ?
      browser_window_testing->GetToolbarView() : NULL;
}

BookmarkBarView* BrowserViewsAccessibilityTest::GetBookmarkBarView() {
  BrowserWindowTesting* browser_window_testing = GetBrowserWindowTesting();
  return browser_window_testing ?
      browser_window_testing->GetBookmarkBarView() : NULL;
}

void BrowserViewsAccessibilityTest::TestViewAccessibilityObject(
    views::View* view,
    std::wstring name,
    int32 role) {
  ASSERT_TRUE(view != NULL);
  TestAccessibilityInfo(view->GetNativeViewAccessible(), name, role);
}

void BrowserViewsAccessibilityTest::TestAccessibilityInfo(IAccessible* acc_obj,
                                                          std::wstring name,
                                                          int32 role) {
  // Verify MSAA Name property.
  BSTR acc_name;
  ASSERT_EQ(S_OK, acc_obj->get_accName(id_self, &acc_name));
  EXPECT_STREQ(name.c_str(), acc_name);
  ::SysFreeString(acc_name);

  // Verify MSAA Role property.
  VARIANT acc_role;
  ::VariantInit(&acc_role);
  ASSERT_EQ(S_OK, acc_obj->get_accRole(id_self, &acc_role));
  EXPECT_EQ(VT_I4, acc_role.vt);
  EXPECT_EQ(role, acc_role.lVal);
  ::VariantClear(&acc_role);
}

// Retrieve accessibility object for main window and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       TestChromeWindowAccObj) {
  BrowserWindow* browser_window = browser()->window();
  ASSERT_TRUE(NULL != browser_window);

  HWND hwnd = browser_window->GetNativeWindow();
  ASSERT_TRUE(NULL != hwnd);

  // Get accessibility object.
  base::win::ScopedComPtr<IAccessible> acc_obj;
  HRESULT hr = ::AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                            reinterpret_cast<void**>(&acc_obj));
  ASSERT_EQ(S_OK, hr);
  ASSERT_TRUE(NULL != acc_obj);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  std::wstring title = base::UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_BROWSER_WINDOW_TITLE_FORMAT,
      ASCIIToUTF16(chrome::kAboutBlankURL)));
  TestAccessibilityInfo(acc_obj, title, ROLE_SYSTEM_WINDOW);
}

// Retrieve accessibility object for non client view and verify accessibility
// info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestNonClientViewAccObj) {
  views::View* non_client_view =
  GetBrowserView()->GetWidget()->non_client_view();

  TestViewAccessibilityObject(non_client_view,
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      ROLE_SYSTEM_WINDOW);
}

// Retrieve accessibility object for browser root view and verify
// accessibility info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestBrowserRootViewAccObj) {
  views::View* browser_root_view = GetBrowserView()->frame()->GetRootView();

  TestViewAccessibilityObject(
      browser_root_view,
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      ROLE_SYSTEM_APPLICATION);
}

// Retrieve accessibility object for browser view and verify accessibility info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestBrowserViewAccObj) {
  // Verify root view MSAA name and role.
  TestViewAccessibilityObject(
      GetBrowserView(),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      ROLE_SYSTEM_CLIENT);
}

// Retrieve accessibility object for toolbar view and verify accessibility info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestToolbarViewAccObj) {
  // Verify toolbar MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView(),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_TOOLBAR)),
      ROLE_SYSTEM_TOOLBAR);
}

// Retrieve accessibility object for Back button and verify accessibility info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestBackButtonAccObj) {
  // Verify Back button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_BACK_BUTTON),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK)),
      ROLE_SYSTEM_BUTTONDROPDOWN);
}

// Retrieve accessibility object for Forward button and verify accessibility
// info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestForwardButtonAccObj) {
  // Verify Forward button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_FORWARD_BUTTON),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD)),
      ROLE_SYSTEM_BUTTONDROPDOWN);
}

// Retrieve accessibility object for Reload button and verify accessibility
// info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestReloadButtonAccObj) {
  // Verify Reload button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_RELOAD_BUTTON),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD)),
      ROLE_SYSTEM_PUSHBUTTON);
}

// Retrieve accessibility object for Home button and verify accessibility info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestHomeButtonAccObj) {
  // Verify Home button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_HOME_BUTTON),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME)),
      ROLE_SYSTEM_PUSHBUTTON);
}

// Retrieve accessibility object for Star button and verify accessibility info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestStarButtonAccObj) {
  // Verify Star button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_STAR_BUTTON),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_STAR)),
      ROLE_SYSTEM_PUSHBUTTON);
}

// Retrieve accessibility object for App menu button and verify accessibility
// info.
// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestAppMenuAccObj) {
  // Verify App menu button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_APP_MENU),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_APP)),
      ROLE_SYSTEM_BUTTONMENU);
}

// http://crbug.com/104132
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       DISABLED_TestBookmarkBarViewAccObj) {
  TestViewAccessibilityObject(
      GetBookmarkBarView(),
      base::UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS)),
      ROLE_SYSTEM_TOOLBAR);
}
