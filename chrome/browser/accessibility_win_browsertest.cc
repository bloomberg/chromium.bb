// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_comptr_win.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

class AccessibilityWinBrowserTest : public InProcessBrowserTest {
 public:
  AccessibilityWinBrowserTest() : screenreader_running_(FALSE) {}

  // InProcessBrowserTest
  void SetUpInProcessBrowserTestFixture();
  void TearDownInProcessBrowserTestFixture();

 protected:
  IAccessible* GetRenderWidgetHostViewClientAccessible();

 private:
  BOOL screenreader_running_;
};

void AccessibilityWinBrowserTest::SetUpInProcessBrowserTestFixture() {
  // This test assumes the windows system-wide SPI_SETSCREENREADER flag is
  // cleared.
  if (SystemParametersInfo(SPI_GETSCREENREADER, 0, &screenreader_running_, 0) &&
      screenreader_running_) {
    // Clear the SPI_SETSCREENREADER flag and notify active applications about
    // the setting change.
    ::SystemParametersInfo(SPI_SETSCREENREADER, FALSE, NULL, 0);
    ::SendNotifyMessage(
        HWND_BROADCAST, WM_SETTINGCHANGE, SPI_GETSCREENREADER, 0);
  }
}

void AccessibilityWinBrowserTest::TearDownInProcessBrowserTestFixture() {
  if (screenreader_running_) {
    // Restore the SPI_SETSCREENREADER flag and notify active applications about
    // the setting change.
    ::SystemParametersInfo(SPI_SETSCREENREADER, TRUE, NULL, 0);
    ::SendNotifyMessage(
        HWND_BROADCAST, WM_SETTINGCHANGE, SPI_GETSCREENREADER, 0);
  }
}

class AccessibleChecker {
 public:
  AccessibleChecker(std::wstring expected_name, int32 expected_role);
  AccessibleChecker(std::wstring expected_name, std::wstring expected_role);

  // Append an AccessibleChecker that verifies accessibility information for
  // a child IAccessible. Order is important.
  void AppendExpectedChild(AccessibleChecker* expected_child);

  // Check that the name and role of the given IAccessible instance and its
  // descendants match the expected names and roles that this object was
  // initialized with.
  void CheckAccessible(IAccessible* accessible);

  typedef std::vector<AccessibleChecker*> AccessibleCheckerVector;

 private:
  void CheckAccessibleName(IAccessible* accessible);
  void CheckAccessibleRole(IAccessible* accessible);
  void CheckAccessibleChildren(IAccessible* accessible);

 private:
  // Expected accessible name. Checked against IAccessible::get_accName.
  std::wstring name_;

  // Expected accessible role. Checked against IAccessible::get_accRole.
  CComVariant role_;

  // Expected accessible children. Checked using IAccessible::get_accChildCount
  // and ::AccessibleChildren.
  AccessibleCheckerVector children_;
};

VARIANT CreateI4Variant(LONG value) {
  VARIANT variant = {0};

  V_VT(&variant) = VT_I4;
  V_I4(&variant) = value;

  return variant;
}

IAccessible* GetAccessibleFromResultVariant(IAccessible* parent, VARIANT *var) {
  switch (V_VT(var)) {
    case VT_DISPATCH:
      return CComQIPtr<IAccessible>(V_DISPATCH(var)).Detach();
      break;

    case VT_I4: {
      CComPtr<IDispatch> dispatch;
      HRESULT hr = parent->get_accChild(CreateI4Variant(V_I4(var)), &dispatch);
      EXPECT_EQ(hr, S_OK);
      return CComQIPtr<IAccessible>(dispatch).Detach();
      break;
    }
  }

  return NULL;
}

// Retrieve the MSAA client accessibility object for the Render Widget Host View
// of the selected tab.
IAccessible*
AccessibilityWinBrowserTest::GetRenderWidgetHostViewClientAccessible() {
  HWND hwnd_render_widget_host_view =
      browser()->GetSelectedTabContents()->GetRenderWidgetHostView()->
          GetNativeView();

  IAccessible* accessible;
  HRESULT hr = AccessibleObjectFromWindow(
      hwnd_render_widget_host_view, OBJID_CLIENT,
      IID_IAccessible, reinterpret_cast<void**>(&accessible));
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(accessible, reinterpret_cast<IAccessible*>(NULL));

  return accessible;
}

AccessibleChecker::AccessibleChecker(
    std::wstring expected_name, int32 expected_role) :
    name_(expected_name),
    role_(expected_role) {
}

AccessibleChecker::AccessibleChecker(
    std::wstring expected_name, std::wstring expected_role) :
    name_(expected_name),
    role_(expected_role.c_str()) {
}

void AccessibleChecker::AppendExpectedChild(
    AccessibleChecker* expected_child) {
  children_.push_back(expected_child);
}

void AccessibleChecker::CheckAccessible(IAccessible* accessible) {
  CheckAccessibleName(accessible);
  CheckAccessibleRole(accessible);
  CheckAccessibleChildren(accessible);
}

void AccessibleChecker::CheckAccessibleName(IAccessible* accessible) {
  CComBSTR name;
  HRESULT hr =
      accessible->get_accName(CreateI4Variant(CHILDID_SELF), &name);

  if (name_.empty()) {
    // If the object doesn't have name S_FALSE should be returned.
    EXPECT_EQ(hr, S_FALSE);
  } else {
    // Test that the correct string was returned.
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(CompareString(LOCALE_NEUTRAL, 0, name, SysStringLen(name),
                  name_.c_str(), name_.length()),
              CSTR_EQUAL);
  }
}

void AccessibleChecker::CheckAccessibleRole(IAccessible* accessible) {
  VARIANT var_role = {0};
  HRESULT hr =
      accessible->get_accRole(CreateI4Variant(CHILDID_SELF), &var_role);
  EXPECT_EQ(hr, S_OK);
  ASSERT_TRUE(role_ == var_role);
}

void AccessibleChecker::CheckAccessibleChildren(IAccessible* parent) {
  LONG child_count = 0;
  HRESULT hr = parent->get_accChildCount(&child_count);
  EXPECT_EQ(hr, S_OK);
  ASSERT_EQ(child_count, children_.size());

  std::auto_ptr<VARIANT> child_array(new VARIANT[child_count]);
  LONG obtained_count = 0;
  hr = AccessibleChildren(parent, 0, child_count,
                          child_array.get(), &obtained_count);
  ASSERT_EQ(hr, S_OK);
  ASSERT_EQ(child_count, obtained_count);

  VARIANT* child = child_array.get();
  for (AccessibleCheckerVector::iterator child_checker = children_.begin();
       child_checker != children_.end();
       ++child_checker, ++child) {
    ScopedComPtr<IAccessible> child_accessible;
    child_accessible.Attach(GetAccessibleFromResultVariant(parent, child));
    (*child_checker)->CheckAccessible(child_accessible);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestRendererAccessibilityTree) {
  // By requesting an accessible chrome will believe a screen reader has been
  // detected.
  ScopedComPtr<IAccessible> document_accessible(
      GetRenderWidgetHostViewClientAccessible());

  // The initial accessible returned should have state STATE_SYSTEM_BUSY while
  // the accessibility tree is being requested from the renderer.
  VARIANT var_state;
  HRESULT hr = document_accessible->
      get_accState(CreateI4Variant(CHILDID_SELF), &var_state);
  EXPECT_EQ(hr, S_OK);
  EXPECT_EQ(V_VT(&var_state), VT_I4);
  EXPECT_EQ(V_I4(&var_state), STATE_SYSTEM_BUSY);

  // Wait for the initial accessibility tree to load.
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  GURL tree_url(
      "data:text/html,<html><head><title>Accessibility Win Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  document_accessible = GetRenderWidgetHostViewClientAccessible();
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));

  AccessibleChecker button_checker(L"push", ROLE_SYSTEM_PUSHBUTTON);
  AccessibleChecker checkbox_checker(L"", ROLE_SYSTEM_CHECKBUTTON);

  AccessibleChecker grouping_checker(L"", L"div");
  grouping_checker.AppendExpectedChild(&button_checker);
  grouping_checker.AppendExpectedChild(&checkbox_checker);

  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT);
  document_checker.AppendExpectedChild(&grouping_checker);

  // Check the accessible tree of the renderer.
  document_checker.CheckAccessible(document_accessible);

  // Check that document accessible has a parent accessible.
  ScopedComPtr<IDispatch> parent_dispatch;
  hr = document_accessible->get_accParent(parent_dispatch.Receive());
  EXPECT_EQ(hr, S_OK);
  EXPECT_NE(parent_dispatch, reinterpret_cast<IDispatch*>(NULL));

  // Navigate to another page.
  GURL about_url("about:");
  ui_test_utils::NavigateToURL(browser(), about_url);

  // Verify that the IAccessible reference still points to a valid object and
  // that calls to its methods fail since the tree is no longer valid after
  // the page navagation.
  // Todo(ctguil): Currently this is giving a false positive because E_FAIL is
  // returned when BrowserAccessibilityManager::RequestAccessibilityInfo fails
  // since the previous render view host connection is lost. Verify that
  // instances are actually marked as invalid once the browse side cache is
  // checked in.
  CComBSTR name;
  hr = document_accessible->get_accName(CreateI4Variant(CHILDID_SELF), &name);
  ASSERT_EQ(E_FAIL, hr);
}
}  // namespace.
