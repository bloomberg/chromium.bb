// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <vector>

#include "base/win/scoped_comptr.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_type.h"
#include "ia2_api_all.h"  // Generated    NOLINT
#include "ISimpleDOMNode.h"  // Generated   NOLINT

using std::auto_ptr;
using std::vector;
using std::wstring;

namespace {

class AccessibilityWinBrowserTest : public InProcessBrowserTest {
 public:
  AccessibilityWinBrowserTest() {}

  // InProcessBrowserTest
  void SetUpInProcessBrowserTestFixture();

 protected:
  IAccessible* GetRendererAccessible();
  void ExecuteScript(wstring script);
};

void AccessibilityWinBrowserTest::SetUpInProcessBrowserTestFixture() {
  // If the mouse happens to be on the document then it will have the unexpected
  // STATE_SYSTEM_HOTTRACKED state. Move it to a non-document location.
  ui_controls::SendMouseMove(0, 0);
}

class AccessibleChecker {
 public:
  AccessibleChecker(
      wstring expected_name,
      int32 expected_role,
      wstring expected_value);
  AccessibleChecker(
      wstring expected_name,
      wstring expected_role,
      wstring expected_value);

  // Append an AccessibleChecker that verifies accessibility information for
  // a child IAccessible. Order is important.
  void AppendExpectedChild(AccessibleChecker* expected_child);

  // Check that the name and role of the given IAccessible instance and its
  // descendants match the expected names and roles that this object was
  // initialized with.
  void CheckAccessible(IAccessible* accessible);

  // Set the expected value for this AccessibleChecker.
  void SetExpectedValue(wstring expected_value);

  // Set the expected state for this AccessibleChecker.
  void SetExpectedState(LONG expected_state);

 private:
  void CheckAccessibleName(IAccessible* accessible);
  void CheckAccessibleRole(IAccessible* accessible);
  void CheckAccessibleValue(IAccessible* accessible);
  void CheckAccessibleState(IAccessible* accessible);
  void CheckAccessibleChildren(IAccessible* accessible);

 private:
  typedef vector<AccessibleChecker*> AccessibleCheckerVector;

  // Expected accessible name. Checked against IAccessible::get_accName.
  wstring name_;

  // Expected accessible role. Checked against IAccessible::get_accRole.
  CComVariant role_;

  // Expected accessible value. Checked against IAccessible::get_accValue.
  wstring value_;

  // Expected accessible state. Checked against IAccessible::get_accState.
  LONG state_;

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
      EXPECT_TRUE(SUCCEEDED(hr));
      return CComQIPtr<IAccessible>(dispatch).Detach();
      break;
    }
  }

  return NULL;
}

HRESULT QueryIAccessible2(IAccessible* accessible, IAccessible2** accessible2) {
  // TODO(ctguil): For some reason querying the IAccessible2 interface from
  // IAccessible fails.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.Receive());
  if (FAILED(hr))
    return hr;

  hr = service_provider->QueryService(IID_IAccessible2, accessible2);
  return hr;
}

// Sets result to true if the child is located in the parent's tree. An
// exhustive search is perform here because we determine equality using
// IAccessible2::get_unique_id which is only supported by the child node.
void AccessibleContainsAccessible(
    IAccessible* parent, IAccessible2* child, bool* result) {
  vector<base::win::ScopedComPtr<IAccessible>> accessible_list;
  accessible_list.push_back(base::win::ScopedComPtr<IAccessible>(parent));

  LONG unique_id;
  HRESULT hr = child->get_uniqueID(&unique_id);
  ASSERT_EQ(S_OK, hr);
  *result = false;

  while (accessible_list.size()) {
    base::win::ScopedComPtr<IAccessible> accessible = accessible_list.back();
    accessible_list.pop_back();

    base::win::ScopedComPtr<IAccessible2> accessible2;
    hr = QueryIAccessible2(accessible, accessible2.Receive());
    if (SUCCEEDED(hr)) {
      LONG child_id;
      accessible2->get_uniqueID(&child_id);
      if (child_id == unique_id) {
        *result = true;
        break;
      }
    }

    LONG child_count;
    hr = accessible->get_accChildCount(&child_count);
    ASSERT_EQ(S_OK, hr);
    if (child_count == 0)
      continue;

    auto_ptr<VARIANT> child_array(new VARIANT[child_count]);
    LONG obtained_count = 0;
    hr = AccessibleChildren(
        accessible, 0, child_count, child_array.get(), &obtained_count);
    ASSERT_EQ(S_OK, hr);
    ASSERT_EQ(child_count, obtained_count);

    for (int index = 0; index < obtained_count; index++) {
      base::win::ScopedComPtr<IAccessible> child_accessible(
        GetAccessibleFromResultVariant(accessible, &child_array.get()[index]));
      if (child_accessible.get()) {
        accessible_list.push_back(
            base::win::ScopedComPtr<IAccessible>(child_accessible));
      }
    }
  }
}

// Retrieve the MSAA client accessibility object for the Render Widget Host View
// of the selected tab.
IAccessible*
AccessibilityWinBrowserTest::GetRendererAccessible() {
  HWND hwnd_render_widget_host_view =
      browser()->GetSelectedTabContents()->GetRenderWidgetHostView()->
          GetNativeView();

  // Invoke windows screen reader detection by sending the WM_GETOBJECT message
  // with kIdCustom as the LPARAM.
  const int32 kIdCustom = 1;
  SendMessage(
      hwnd_render_widget_host_view, WM_GETOBJECT, OBJID_CLIENT, kIdCustom);

  IAccessible* accessible;
  HRESULT hr = AccessibleObjectFromWindow(
      hwnd_render_widget_host_view, OBJID_CLIENT,
      IID_IAccessible, reinterpret_cast<void**>(&accessible));
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(accessible, reinterpret_cast<IAccessible*>(NULL));

  return accessible;
}

void AccessibilityWinBrowserTest::ExecuteScript(wstring script) {
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", script);
}

AccessibleChecker::AccessibleChecker(
    wstring expected_name, int32 expected_role, wstring expected_value) :
    name_(expected_name),
    role_(expected_role),
    value_(expected_value),
    state_(-1) {
}

AccessibleChecker::AccessibleChecker(
    wstring expected_name, wstring expected_role, wstring expected_value) :
    name_(expected_name),
    role_(expected_role.c_str()),
    value_(expected_value),
    state_(-1) {
}

void AccessibleChecker::AppendExpectedChild(
    AccessibleChecker* expected_child) {
  children_.push_back(expected_child);
}

void AccessibleChecker::CheckAccessible(IAccessible* accessible) {
  CheckAccessibleName(accessible);
  CheckAccessibleRole(accessible);
  CheckAccessibleValue(accessible);
  CheckAccessibleState(accessible);
  CheckAccessibleChildren(accessible);
}

void AccessibleChecker::SetExpectedValue(wstring expected_value) {
  value_ = expected_value;
}

void AccessibleChecker::SetExpectedState(LONG expected_state) {
  state_ = expected_state;
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
    EXPECT_EQ(S_OK, hr);
    EXPECT_STREQ(name_.c_str(),
                 wstring(name.m_str, SysStringLen(name)).c_str());
  }
}

void AccessibleChecker::CheckAccessibleRole(IAccessible* accessible) {
  VARIANT var_role = {0};
  HRESULT hr =
      accessible->get_accRole(CreateI4Variant(CHILDID_SELF), &var_role);
  ASSERT_EQ(S_OK, hr);
  EXPECT_TRUE(role_ == var_role);
}

void AccessibleChecker::CheckAccessibleValue(IAccessible* accessible) {
  CComBSTR value;
  HRESULT hr =
      accessible->get_accValue(CreateI4Variant(CHILDID_SELF), &value);
  EXPECT_EQ(S_OK, hr);

  // Test that the correct string was returned.
  EXPECT_STREQ(value_.c_str(),
               wstring(value.m_str, SysStringLen(value)).c_str());
}

void AccessibleChecker::CheckAccessibleState(IAccessible* accessible) {
  if (state_ < 0)
    return;

  VARIANT var_state = {0};
  HRESULT hr =
      accessible->get_accState(CreateI4Variant(CHILDID_SELF), &var_state);
  EXPECT_EQ(S_OK, hr);
  ASSERT_EQ(VT_I4, V_VT(&var_state));
  EXPECT_EQ(state_, V_I4(&var_state));
}

void AccessibleChecker::CheckAccessibleChildren(IAccessible* parent) {
  LONG child_count = 0;
  HRESULT hr = parent->get_accChildCount(&child_count);
  EXPECT_EQ(S_OK, hr);
  ASSERT_EQ(child_count, children_.size());

  auto_ptr<VARIANT> child_array(new VARIANT[child_count]);
  LONG obtained_count = 0;
  hr = AccessibleChildren(parent, 0, child_count,
                          child_array.get(), &obtained_count);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(child_count, obtained_count);

  VARIANT* child = child_array.get();
  for (AccessibleCheckerVector::iterator child_checker = children_.begin();
       child_checker != children_.end();
       ++child_checker, ++child) {
    base::win::ScopedComPtr<IAccessible> child_accessible;
    child_accessible.Attach(GetAccessibleFromResultVariant(parent, child));
    ASSERT_TRUE(child_accessible.get());
    (*child_checker)->CheckAccessible(child_accessible);
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestRendererAccessibilityTree) {
  // The initial accessible returned should have state STATE_SYSTEM_BUSY while
  // the accessibility tree is being requested from the renderer.
  AccessibleChecker document1_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document1_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED |
      STATE_SYSTEM_BUSY);
  document1_checker.CheckAccessible(GetRendererAccessible());

  // Wait for the initial accessibility tree to load. Busy state should clear.
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);
  document1_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document1_checker.CheckAccessible(GetRendererAccessible());

  GURL tree_url(
      "data:text/html,<html><head><title>Accessibility Win Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker button_checker(L"push", ROLE_SYSTEM_PUSHBUTTON, L"push");
  AccessibleChecker checkbox_checker(L"", ROLE_SYSTEM_CHECKBUTTON, L"");
  AccessibleChecker body_checker(L"", L"body", L"");
  AccessibleChecker document2_checker(
    L"Accessibility Win Test", ROLE_SYSTEM_DOCUMENT, L"");
  body_checker.AppendExpectedChild(&button_checker);
  body_checker.AppendExpectedChild(&checkbox_checker);
  document2_checker.AppendExpectedChild(&body_checker);
  document2_checker.CheckAccessible(GetRendererAccessible());

  // Check that document accessible has a parent accessible.
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));
  base::win::ScopedComPtr<IDispatch> parent_dispatch;
  HRESULT hr = document_accessible->get_accParent(parent_dispatch.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(parent_dispatch, reinterpret_cast<IDispatch*>(NULL));

  // Navigate to another page.
  GURL about_url("about:");
  ui_test_utils::NavigateToURL(browser(), about_url);

  // Verify that the IAccessible reference still points to a valid object and
  // that calls to its methods fail since the tree is no longer valid after
  // the page navagation.
  CComBSTR name;
  hr = document_accessible->get_accName(CreateI4Variant(CHILDID_SELF), &name);
  ASSERT_EQ(E_FAIL, hr);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationActiveDescendantChanged) {
  GURL tree_url("data:text/html,<ul tabindex='-1' role='radiogroup'><li id='li'"
      ">li</li></ul>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker list_marker_checker(L"", ROLE_SYSTEM_LISTITEM, L"\x2022");
  AccessibleChecker static_text_checker(L"li", ROLE_SYSTEM_TEXT, L"");
  AccessibleChecker list_item_checker(L"", ROLE_SYSTEM_LISTITEM, L"");
  list_item_checker.SetExpectedState(
      STATE_SYSTEM_READONLY);
  AccessibleChecker radio_group_checker(L"", ROLE_SYSTEM_GROUPING, L"");
  radio_group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY);
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  list_item_checker.AppendExpectedChild(&list_marker_checker);
  list_item_checker.AppendExpectedChild(&static_text_checker);
  radio_group_checker.AppendExpectedChild(&list_item_checker);
  document_checker.AppendExpectedChild(&radio_group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set focus to the radio group.
  ExecuteScript(L"document.body.children[0].focus()");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  radio_group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the active descendant of the radio group
  ExecuteScript(
      L"document.body.children[0].setAttribute('aria-activedescendant', 'li')");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  list_item_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSED);
  radio_group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationCheckedStateChanged) {
  GURL tree_url("data:text/html,<body><input type='checkbox' /></body>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker checkbox_checker(L"", ROLE_SYSTEM_CHECKBUTTON, L"");
  checkbox_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY);
  AccessibleChecker body_checker(L"", L"body", L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  body_checker.AppendExpectedChild(&checkbox_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Check the checkbox.
  ExecuteScript(L"document.body.children[0].checked=true");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  checkbox_checker.SetExpectedState(
      STATE_SYSTEM_CHECKED | STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  GURL tree_url(
      "data:text/html,<body role=group></body>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker body_checker(L"", L"body", L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  ExecuteScript(L"document.body.innerHTML='<b>new text</b>'");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker text_checker(L"new text", ROLE_SYSTEM_TEXT, L"");
  body_checker.AppendExpectedChild(&text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged2) {
  // The role attribute causes the node to be in the accessibility tree.
  GURL tree_url(
      "data:text/html,<div role=group style='visibility: hidden'>text"
      "</div>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the accessible tree of the browser.
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  ExecuteScript(L"document.body.children[0].style.visibility='visible'");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker static_text_checker(L"text", ROLE_SYSTEM_TEXT, L"");
  AccessibleChecker div_checker(L"", L"div", L"");
  document_checker.AppendExpectedChild(&div_checker);
  div_checker.AppendExpectedChild(&static_text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationFocusChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  GURL tree_url(
      "data:text/html,<div role=group tabindex='-1'></div>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker div_checker(L"", L"div", L"");
  div_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_OFFSCREEN | STATE_SYSTEM_READONLY);
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.AppendExpectedChild(&div_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Focus the div in the document
  ExecuteScript(L"document.body.children[0].focus()");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  div_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Focus the document accessible. This will un-focus the current node.
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));
  HRESULT hr = document_accessible->accSelect(
    SELFLAG_TAKEFOCUS, CreateI4Variant(CHILDID_SELF));
  ASSERT_EQ(S_OK, hr);
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  div_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_READONLY);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationValueChanged) {
  GURL tree_url("data:text/html,<body><input type='text' value='old value'/>"
      "</body>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check the browser's copy of the renderer accessibility tree.

  AccessibleChecker text_field_div_checker(L"", L"div", L"");
  AccessibleChecker text_field_checker(L"", ROLE_SYSTEM_TEXT, L"old value");
  text_field_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(L"", L"body", L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  text_field_checker.AppendExpectedChild(&text_field_div_checker);
  body_checker.AppendExpectedChild(&text_field_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the value of the text control
  ExecuteScript(L"document.body.children[0].value='new value'");
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Check that the accessibility tree of the browser has been updated.
  text_field_checker.SetExpectedValue(L"new value");
  document_checker.CheckAccessible(GetRendererAccessible());
}

// FAILS crbug.com/54220
// This test verifies that browser-side cache of the renderer accessibility
// tree is reachable from the browser's tree. Tools that analyze windows
// accessibility trees like AccExplorer32 should be able to drill into the
// cached renderer accessibility tree.
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_ContainsRendererAccessibilityTree) {
  GURL tree_url("data:text/html,<body><input type='checkbox' /></body>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Get the accessibility object for the browser window.
  HWND browser_hwnd = browser()->window()->GetNativeHandle();
  base::win::ScopedComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindow(
      browser_hwnd,
      OBJID_WINDOW,
      IID_IAccessible,
      reinterpret_cast<void**>(browser_accessible.Receive()));
  ASSERT_EQ(S_OK, hr);

  // Get the accessibility object for the renderer client document.
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));
  base::win::ScopedComPtr<IAccessible2> document_accessible2;
  hr = QueryIAccessible2(document_accessible, document_accessible2.Receive());
  ASSERT_EQ(S_OK, hr);

  // TODO(ctguil): Pointer comparison of retrieved IAccessible pointers dosen't
  // seem to work for here. Perhaps make IAccessible2 available in views to make
  // unique id comparison available.
  bool found = false;
  base::win::ScopedComPtr<IAccessible> parent = document_accessible;
  while (parent.get()) {
    base::win::ScopedComPtr<IDispatch> parent_dispatch;
    hr = parent->get_accParent(parent_dispatch.Receive());
    ASSERT_TRUE(SUCCEEDED(hr));
    if (!parent_dispatch.get()) {
      ASSERT_EQ(hr, S_FALSE);
      break;
    }

    parent.Release();
    hr = parent_dispatch.QueryInterface(parent.Receive());
    ASSERT_EQ(S_OK, hr);

    if (parent.get() == browser_accessible.get()) {
      found = true;
      break;
    }
  }

  // If pointer comparison fails resort to the exhuasive search that can use
  // IAccessible2::get_unique_id for equality comparison.
  if (!found) {
    AccessibleContainsAccessible(
        browser_accessible, document_accessible2, &found);
  }

  ASSERT_EQ(found, true);
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       SupportsISimpleDOM) {
  GURL tree_url("data:text/html,<body><input type='checkbox' /></body>");
  browser()->OpenURL(tree_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
  GetRendererAccessible();
  ui_test_utils::WaitForNotification(
      NotificationType::RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED);

  // Get the IAccessible object for the document.
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));

  // Get the ISimpleDOM object for the document.
  base::win::ScopedComPtr<IServiceProvider> service_provider;
  HRESULT hr = static_cast<IAccessible*>(document_accessible)->QueryInterface(
      service_provider.Receive());
  ASSERT_EQ(S_OK, hr);
  const GUID refguid = {0x0c539790, 0x12e4, 0x11cf,
                        0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8};
  base::win::ScopedComPtr<ISimpleDOMNode> document_isimpledomnode;
  hr = static_cast<IServiceProvider *>(service_provider)->QueryService(
      refguid, IID_ISimpleDOMNode,
      reinterpret_cast<void**>(document_isimpledomnode.Receive()));
  ASSERT_EQ(S_OK, hr);

  BSTR node_name;
  short name_space_id;  // NOLINT
  BSTR node_value;
  unsigned int num_children;
  unsigned int unique_id;
  unsigned short node_type;  // NOLINT
  hr = document_isimpledomnode->get_nodeInfo(
      &node_name, &name_space_id, &node_value, &num_children, &unique_id,
      &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(NODETYPE_DOCUMENT, node_type);
  EXPECT_EQ(1, num_children);

  base::win::ScopedComPtr<ISimpleDOMNode> body_isimpledomnode;
  hr = document_isimpledomnode->get_firstChild(
      body_isimpledomnode.Receive());
  ASSERT_EQ(S_OK, hr);
  hr = body_isimpledomnode->get_nodeInfo(
      &node_name, &name_space_id, &node_value, &num_children, &unique_id,
      &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_STREQ(L"body", wstring(node_name, SysStringLen(node_name)).c_str());
  EXPECT_EQ(NODETYPE_ELEMENT, node_type);
  EXPECT_EQ(1, num_children);

  base::win::ScopedComPtr<ISimpleDOMNode> checkbox_isimpledomnode;
  hr = body_isimpledomnode->get_firstChild(
      checkbox_isimpledomnode.Receive());
  ASSERT_EQ(S_OK, hr);
  hr = checkbox_isimpledomnode->get_nodeInfo(
      &node_name, &name_space_id, &node_value, &num_children, &unique_id,
      &node_type);
  ASSERT_EQ(S_OK, hr);
  EXPECT_STREQ(L"input", wstring(node_name, SysStringLen(node_name)).c_str());
  EXPECT_EQ(NODETYPE_ELEMENT, node_type);
  EXPECT_EQ(0, num_children);
}
}  // namespace.
