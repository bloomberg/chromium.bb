// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <oleacc.h>

#include "base/scoped_comptr_win.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/accessibility/native_view_accessibility_win.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"
#include "views/window/window.h"

namespace {

VARIANT id_self = {VT_I4, CHILDID_SELF};

// Dummy class to force creation of ATL module, needed by COM to instantiate
// NativeViewAccessibilityWin.
class TestAtlModule : public CAtlDllModuleT<TestAtlModule> {};
TestAtlModule test_atl_module_;

}  // namespace

class BrowserViewsAccessibilityTest : public InProcessBrowserTest {
 public:
  BrowserViewsAccessibilityTest() {
    ::CoInitialize(NULL);
  }

  ~BrowserViewsAccessibilityTest() {
    ::CoUninitialize();
  }

  // Retrieves an instance of BrowserWindowTesting
  BrowserWindowTesting* GetBrowserWindowTesting() {
    BrowserWindow* browser_window = browser()->window();

    if (!browser_window)
      return NULL;

    return browser_window->GetBrowserWindowTesting();
  }

  // Retrieve an instance of BrowserView
  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForNativeWindow(
               browser()->window()->GetNativeHandle());
  }

  // Retrieves and initializes an instance of ToolbarView.
  ToolbarView* GetToolbarView() {
    BrowserWindowTesting* browser_window_testing = GetBrowserWindowTesting();

    if (!browser_window_testing)
      return NULL;

    return browser_window_testing->GetToolbarView();
  }

  // Retrieves and initializes an instance of BookmarkBarView.
  BookmarkBarView* GetBookmarkBarView() {
    BrowserWindowTesting* browser_window_testing = GetBrowserWindowTesting();

    if (!browser_window_testing)
      return NULL;

    return browser_window_testing->GetBookmarkBarView();
  }

  // Retrieves and verifies the accessibility object for the given View.
  void TestViewAccessibilityObject(views::View* view, std::wstring name,
                                   int32 role) {
    ASSERT_TRUE(NULL != view);

    TestAccessibilityInfo(
        views::NativeViewAccessibilityWin::GetAccessibleForView(view),
        name,
        role);
  }


  // Verifies MSAA Name and Role properties of the given IAccessible.
  void TestAccessibilityInfo(IAccessible* acc_obj, std::wstring name,
                             int32 role) {
    // Verify MSAA Name property.
    BSTR acc_name;

    HRESULT hr = acc_obj->get_accName(id_self, &acc_name);
    ASSERT_EQ(S_OK, hr);
    EXPECT_STREQ(acc_name, name.c_str());

    // Verify MSAA Role property.
    VARIANT acc_role;
    ::VariantInit(&acc_role);

    hr = acc_obj->get_accRole(id_self, &acc_role);
    ASSERT_EQ(S_OK, hr);
    EXPECT_EQ(VT_I4, acc_role.vt);
    EXPECT_EQ(role, acc_role.lVal);

    ::VariantClear(&acc_role);
    ::SysFreeString(acc_name);
  }
};

// Retrieve accessibility object for main window and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       TestChromeWindowAccObj) {
  BrowserWindow* browser_window = browser()->window();
  ASSERT_TRUE(NULL != browser_window);

  HWND hwnd = browser_window->GetNativeHandle();
  ASSERT_TRUE(NULL != hwnd);

  // Get accessibility object.
  ScopedComPtr<IAccessible> acc_obj;
  HRESULT hr = ::AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
                                            reinterpret_cast<void**>(&acc_obj));
  ASSERT_EQ(S_OK, hr);
  ASSERT_TRUE(NULL != acc_obj);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  std::wstring title = UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_BROWSER_WINDOW_TITLE_FORMAT,
      ASCIIToUTF16(chrome::kAboutBlankURL)));
  TestAccessibilityInfo(acc_obj, title, ROLE_SYSTEM_WINDOW);
}

// Retrieve accessibility object for non client view and verify accessibility
// info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestNonClientViewAccObj) {
  views::View* non_client_view =
  GetBrowserView()->GetWindow()->GetNonClientView();

  TestViewAccessibilityObject(non_client_view,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      ROLE_SYSTEM_WINDOW);
}

// Retrieve accessibility object for browser root view and verify
// accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       TestBrowserRootViewAccObj) {
  views::View* browser_root_view =
      GetBrowserView()->frame()->GetFrameView()->GetRootView();

  TestViewAccessibilityObject(
      browser_root_view,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      ROLE_SYSTEM_APPLICATION);
}

// Retrieve accessibility object for browser view and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestBrowserViewAccObj) {
  // Verify root view MSAA name and role.
  TestViewAccessibilityObject(
      GetBrowserView(),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      ROLE_SYSTEM_CLIENT);
}

// Retrieve accessibility object for toolbar view and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestToolbarViewAccObj) {
  // Verify toolbar MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView(),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_TOOLBAR)),
      ROLE_SYSTEM_TOOLBAR);
}

// Retrieve accessibility object for Back button and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestBackButtonAccObj) {
  // Verify Back button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_BACK_BUTTON),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK)),
      ROLE_SYSTEM_BUTTONDROPDOWN);
}

// Retrieve accessibility object for Forward button and verify accessibility
// info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestForwardButtonAccObj) {
  // Verify Forward button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_FORWARD_BUTTON),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD)),
      ROLE_SYSTEM_BUTTONDROPDOWN);
}

// Retrieve accessibility object for Reload button and verify accessibility
// info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestReloadButtonAccObj) {
  // Verify Reload button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_RELOAD_BUTTON),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD)),
      ROLE_SYSTEM_PUSHBUTTON);
}

// Retrieve accessibility object for Home button and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestHomeButtonAccObj) {
  // Verify Home button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_HOME_BUTTON),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME)),
      ROLE_SYSTEM_PUSHBUTTON);
}

// Retrieve accessibility object for Star button and verify accessibility info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       TestStarButtonAccObj) {
  // Verify Star button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_STAR_BUTTON),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_STAR)),
      ROLE_SYSTEM_PUSHBUTTON);
}

// Retrieve accessibility object for App menu button and verify accessibility
// info.
IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest, TestAppMenuAccObj) {
  // Verify App menu button MSAA name and role.
  TestViewAccessibilityObject(
      GetToolbarView()->GetViewByID(VIEW_ID_APP_MENU),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_APP)),
      ROLE_SYSTEM_BUTTONMENU);
}

IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       TestBookmarkBarViewAccObj) {
  TestViewAccessibilityObject(
      GetBookmarkBarView(),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS)),
      ROLE_SYSTEM_TOOLBAR);
}

IN_PROC_BROWSER_TEST_F(BrowserViewsAccessibilityTest,
                       TestAboutChromeViewAccObj) {
  //  Firstly, test that the WindowDelegate got updated.
  views::Window* aboutChromeWindow =
      GetBrowserView()->DoShowAboutChromeDialog();
  EXPECT_STREQ(
      aboutChromeWindow->GetDelegate()->GetWindowTitle().c_str(),
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_TITLE)).c_str());
  EXPECT_EQ(aboutChromeWindow->GetDelegate()->GetAccessibleRole(),
            ui::AccessibilityTypes::ROLE_DIALOG);

  // Also test the accessibility object directly.
  IAccessible* acc_obj = NULL;
  HRESULT hr =
    ::AccessibleObjectFromWindow(aboutChromeWindow->GetNativeWindow(),
                                 OBJID_CLIENT,
                                 IID_IAccessible,
                                 reinterpret_cast<void**>(&acc_obj));
  ASSERT_EQ(S_OK, hr);
  ASSERT_TRUE(NULL != acc_obj);

  TestAccessibilityInfo(
      acc_obj,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_ABOUT_CHROME_TITLE)),
      ROLE_SYSTEM_DIALOG);

  acc_obj->Release();
}
