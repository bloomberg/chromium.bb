// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_test_utils_win.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"

using std::auto_ptr;
using std::string;
using std::vector;
using std::wstring;

namespace content {

namespace {

class AccessibilityWinBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityWinBrowserTest() {}

 protected:
  void LoadInitialAccessibilityTreeFromHtml(string html);
  IAccessible* GetRendererAccessible();
  void ExecuteScript(wstring script);
};

class AccessibleChecker {
 public:
  AccessibleChecker(
      wstring expected_name,
      int32 expected_role,
      wstring expected_value);
  AccessibleChecker(
      wstring expected_name,
      wstring expected_role,
      int32 expected_ia2_role,
      wstring expected_value);
  AccessibleChecker(
      wstring expected_name,
      int32 expected_role,
      int32 expected_ia2_role,
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
  void CheckIA2Role(IAccessible* accessible);
  void CheckAccessibleValue(IAccessible* accessible);
  void CheckAccessibleState(IAccessible* accessible);
  void CheckAccessibleChildren(IAccessible* accessible);
  string16 RoleVariantToString(VARIANT* role_variant);

 private:
  typedef vector<AccessibleChecker*> AccessibleCheckerVector;

  // Expected accessible name. Checked against IAccessible::get_accName.
  wstring name_;

  // Expected accessible role. Checked against IAccessible::get_accRole.
  CComVariant role_;

  // Expected IAccessible2 role. Checked against IAccessible2::role.
  int32 ia2_role_;

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

// Recursively search through all of the descendants reachable from an
// IAccessible node and return true if we find one with the given role
// and name.
void RecursiveFindNodeInAccessibilityTree(
    IAccessible* node,
    int32 expected_role,
    const wstring& expected_name,
    int32 depth,
    bool* found) {
  CComBSTR name_bstr;
  node->get_accName(CreateI4Variant(CHILDID_SELF), &name_bstr);
  wstring name(name_bstr.m_str, SysStringLen(name_bstr));
  VARIANT role = {0};
  node->get_accRole(CreateI4Variant(CHILDID_SELF), &role);

  // Print the accessibility tree as we go, because if this test fails
  // on the bots, this is really helpful in figuring out why.
  for (int i = 0; i < depth; i++) {
    printf("  ");
  }
  printf("role=%d name=%s\n", role.lVal, WideToUTF8(name).c_str());

  if (expected_role == role.lVal && expected_name == name) {
    *found = true;
    return;
  }

  LONG child_count = 0;
  HRESULT hr = node->get_accChildCount(&child_count);
  ASSERT_EQ(S_OK, hr);

  scoped_array<VARIANT> child_array(new VARIANT[child_count]);
  LONG obtained_count = 0;
  hr = AccessibleChildren(
      node, 0, child_count, child_array.get(), &obtained_count);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(child_count, obtained_count);

  for (int index = 0; index < obtained_count; index++) {
    base::win::ScopedComPtr<IAccessible> child_accessible(
        GetAccessibleFromResultVariant(node, &child_array.get()[index]));
    if (child_accessible.get()) {
      RecursiveFindNodeInAccessibilityTree(
          child_accessible.get(), expected_role, expected_name, depth + 1,
          found);
      if (*found)
        return;
    }
  }
}

void AccessibilityWinBrowserTest::LoadInitialAccessibilityTreeFromHtml(
    string html) {
  // Load the html using a data url and wait for the navigation to finish.
  GURL html_data_url(string("data:text/html,") + html);
  NavigateToURL(shell(), html_data_url);

  // At this point, renderer accessibility is off and the page has completed
  // loading. (Both of these must be strictly true or there will be test
  // flakiness.)  Now call GetRendererAccessible, which will trigger
  // changing the accessibility mode to AccessibilityModeComplete. When
  // the renderer switches accessibility on, it will send a Layout Complete
  // accessibility notification containing the full accessibility tree, which
  // we can wait for.
  WindowedNotificationObserver tree_updated_observer(
      NOTIFICATION_ACCESSIBILITY_LAYOUT_COMPLETE,
      NotificationService::AllSources());
  GetRendererAccessible();
  tree_updated_observer.Wait();
}

// Retrieve the MSAA client accessibility object for the Render Widget Host View
// of the selected tab.
IAccessible*
AccessibilityWinBrowserTest::GetRendererAccessible() {
  HWND hwnd_render_widget_host_view =
      shell()->web_contents()->GetRenderWidgetHostView()->GetNativeView();

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
  shell()->web_contents()->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(L"", script);
}

// This constructor can be used if IA2 role will be the same as MSAA role
AccessibleChecker::AccessibleChecker(
    wstring expected_name, int32 expected_role, wstring expected_value) :
    name_(expected_name),
    role_(expected_role),
    ia2_role_(expected_role),
    value_(expected_value),
    state_(-1) {
}

AccessibleChecker::AccessibleChecker(wstring expected_name,
                                     int32 expected_role,
                                     int32 expected_ia2_role,
                                     wstring expected_value) :
    name_(expected_name),
    role_(expected_role),
    ia2_role_(expected_ia2_role),
    value_(expected_value),
    state_(-1) {
}

AccessibleChecker::AccessibleChecker(wstring expected_name,
                                     wstring expected_role,
                                     int32 expected_ia2_role,
                                     wstring expected_value) :
    name_(expected_name),
    role_(expected_role.c_str()),
    ia2_role_(expected_ia2_role),
    value_(expected_value),
    state_(-1) {
}

void AccessibleChecker::AppendExpectedChild(
    AccessibleChecker* expected_child) {
  children_.push_back(expected_child);
}

void AccessibleChecker::CheckAccessible(IAccessible* accessible) {
  SCOPED_TRACE(base::StringPrintf(
      "while checking %s",
      UTF16ToUTF8(RoleVariantToString(&role_)).c_str()));
  CheckAccessibleName(accessible);
  CheckAccessibleRole(accessible);
  CheckIA2Role(accessible);
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
  EXPECT_EQ(role_, var_role);
  if (role_ != var_role) {
    LOG(ERROR) << "Expected role: " << RoleVariantToString(&role_);
    LOG(ERROR) << "Got role: " << RoleVariantToString(&var_role);
  }
}

void AccessibleChecker::CheckIA2Role(IAccessible* accessible) {
  base::win::ScopedComPtr<IAccessible2> accessible2;
  HRESULT hr = QueryIAccessible2(accessible, accessible2.Receive());
  ASSERT_EQ(S_OK, hr);
  long ia2_role = 0;
  hr = accessible2->role(&ia2_role);
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(ia2_role_, ia2_role);
  if (ia2_role_ != ia2_role) {
    LOG(ERROR) << "Expected ia2 role: " <<
        IAccessible2RoleToString(ia2_role_);
    LOG(ERROR) << "Got ia2 role: " <<
        IAccessible2RoleToString(ia2_role);
  }
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
  if (state_ != V_I4(&var_state)) {
    LOG(ERROR) << "Expected state: " <<
        IAccessibleStateToString(state_);
    LOG(ERROR) << "Got state: " <<
        IAccessibleStateToString(V_I4(&var_state));
  }
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

string16 AccessibleChecker::RoleVariantToString(VARIANT* role_variant) {
  if (V_VT(role_variant) == VT_I4)
    return IAccessibleRoleToString(V_I4(role_variant));
  else if (V_VT(role_variant) == VT_BSTR)
    return string16(V_BSTR(role_variant), SysStringLen(V_BSTR(role_variant)));
  return string16();
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestBusyAccessibilityTree) {
  NavigateToURL(shell(), GURL(chrome::kAboutBlankURL));

  // The initial accessible returned should have state STATE_SYSTEM_BUSY while
  // the accessibility tree is being requested from the renderer.
  AccessibleChecker document1_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document1_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED |
      STATE_SYSTEM_BUSY);
  document1_checker.CheckAccessible(GetRendererAccessible());
}

// Flaky, http://crbug.com/167320 .
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestRendererAccessibilityTree) {
  LoadInitialAccessibilityTreeFromHtml(
      "<html><head><title>Accessibility Win Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker button_checker(L"push", ROLE_SYSTEM_PUSHBUTTON, L"");
  AccessibleChecker checkbox_checker(L"", ROLE_SYSTEM_CHECKBUTTON, L"");
  AccessibleChecker body_checker(L"", L"body", IA2_ROLE_SECTION, L"");
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
  NavigateToURL(shell(), GURL(chrome::kAboutBlankURL));

  // Verify that the IAccessible reference still points to a valid object and
  // that calls to its methods fail since the tree is no longer valid after
  // the page navagation.
  CComBSTR name;
  hr = document_accessible->get_accName(CreateI4Variant(CHILDID_SELF), &name);
  ASSERT_EQ(E_FAIL, hr);
}

// Periodically failing.  See crbug.com/145537
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestNotificationActiveDescendantChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<ul tabindex='-1' role='radiogroup' aria-label='ul'>"
      "<li id='li'>li</li></ul>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker list_marker_checker(L"\x2022", ROLE_SYSTEM_TEXT, L"");
  AccessibleChecker static_text_checker(L"li", ROLE_SYSTEM_TEXT, L"");
  AccessibleChecker list_item_checker(L"", ROLE_SYSTEM_LISTITEM, L"");
  list_item_checker.SetExpectedState(
      STATE_SYSTEM_READONLY);
  AccessibleChecker radio_group_checker(L"ul", ROLE_SYSTEM_GROUPING,
      IA2_ROLE_SECTION, L"");
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  list_item_checker.AppendExpectedChild(&list_marker_checker);
  list_item_checker.AppendExpectedChild(&static_text_checker);
  radio_group_checker.AppendExpectedChild(&list_item_checker);
  document_checker.AppendExpectedChild(&radio_group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set focus to the radio group.
  WindowedNotificationObserver tree_updated_observer(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(L"document.body.children[0].focus()");
  tree_updated_observer.Wait();

  // Check that the accessibility tree of the browser has been updated.
  radio_group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the active descendant of the radio group
  WindowedNotificationObserver tree_updated_observer3(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(
      L"document.body.children[0].setAttribute('aria-activedescendant', 'li')");
  tree_updated_observer3.Wait();

  // Check that the accessibility tree of the browser has been updated.
  list_item_checker.SetExpectedState(
      STATE_SYSTEM_READONLY | STATE_SYSTEM_FOCUSED);
  radio_group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationCheckedStateChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='checkbox' /></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker checkbox_checker(L"", ROLE_SYSTEM_CHECKBUTTON, L"");
  checkbox_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(L"", L"body", IA2_ROLE_SECTION, L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  body_checker.AppendExpectedChild(&checkbox_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Check the checkbox.
  WindowedNotificationObserver tree_updated_observer(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(L"document.body.children[0].checked=true");
  tree_updated_observer.Wait();

  // Check that the accessibility tree of the browser has been updated.
  checkbox_checker.SetExpectedState(
      STATE_SYSTEM_CHECKED | STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml("<body role=group></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker group_checker(L"", ROLE_SYSTEM_GROUPING, L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.AppendExpectedChild(&group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  WindowedNotificationObserver tree_updated_observer(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(L"document.body.innerHTML='<b>new text</b>'");
  tree_updated_observer.Wait();

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker text_checker(L"new text", ROLE_SYSTEM_TEXT, L"");
  group_checker.AppendExpectedChild(&text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationChildrenChanged2) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml(
      "<div role=group style='visibility: hidden'>text</div>");

  // Check the accessible tree of the browser.
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.CheckAccessible(GetRendererAccessible());

  // Change the children of the document body.
  WindowedNotificationObserver tree_updated_observer2(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(L"document.body.children[0].style.visibility='visible'");
  tree_updated_observer2.Wait();

  // Check that the accessibility tree of the browser has been updated.
  AccessibleChecker static_text_checker(L"text", ROLE_SYSTEM_TEXT, L"");
  AccessibleChecker group_checker(L"", ROLE_SYSTEM_GROUPING, L"");
  document_checker.AppendExpectedChild(&group_checker);
  group_checker.AppendExpectedChild(&static_text_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationFocusChanged) {
  // The role attribute causes the node to be in the accessibility tree.
  LoadInitialAccessibilityTreeFromHtml("<div role=group tabindex='-1'></div>");

  // Check the browser's copy of the renderer accessibility tree.
  SCOPED_TRACE("Check initial tree");
  AccessibleChecker group_checker(L"", ROLE_SYSTEM_GROUPING, L"");
  group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_OFFSCREEN);
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.AppendExpectedChild(&group_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Focus the div in the document
  WindowedNotificationObserver tree_updated_observer(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(L"document.body.children[0].focus()");
  tree_updated_observer.Wait();

  // Check that the accessibility tree of the browser has been updated.
  SCOPED_TRACE("Check updated tree after focusing div");
  group_checker.SetExpectedState(
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_FOCUSED);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Focus the document accessible. This will un-focus the current node.
  WindowedNotificationObserver tree_updated_observer2(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  base::win::ScopedComPtr<IAccessible> document_accessible(
      GetRendererAccessible());
  ASSERT_NE(document_accessible.get(), reinterpret_cast<IAccessible*>(NULL));
  HRESULT hr = document_accessible->accSelect(
    SELFLAG_TAKEFOCUS, CreateI4Variant(CHILDID_SELF));
  ASSERT_EQ(S_OK, hr);
  tree_updated_observer2.Wait();

  // Check that the accessibility tree of the browser has been updated.
  SCOPED_TRACE("Check updated tree after focusing document again");
  group_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       TestNotificationValueChanged) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='text' value='old value'/></body>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker text_field_checker(L"", ROLE_SYSTEM_TEXT, L"old value");
  text_field_checker.SetExpectedState(STATE_SYSTEM_FOCUSABLE);
  AccessibleChecker body_checker(L"", L"body", IA2_ROLE_SECTION, L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  body_checker.AppendExpectedChild(&text_field_checker);
  document_checker.AppendExpectedChild(&body_checker);
  document_checker.CheckAccessible(GetRendererAccessible());

  // Set the value of the text control
  WindowedNotificationObserver tree_updated_observer(
      NOTIFICATION_ACCESSIBILITY_OTHER,
      NotificationService::AllSources());
  ExecuteScript(L"document.body.children[0].value='new value'");
  tree_updated_observer.Wait();

  // Check that the accessibility tree of the browser has been updated.
  text_field_checker.SetExpectedValue(L"new value");
  document_checker.CheckAccessible(GetRendererAccessible());
}

// This test verifies that the web content's accessibility tree is a
// descendant of the main browser window's accessibility tree, so that
// tools like AccExplorer32 or AccProbe can be used to examine Chrome's
// accessibility support.
//
// If you made a change and this test now fails, check that the NativeViewHost
// that wraps the tab contents returns the IAccessible implementation
// provided by RenderWidgetHostViewWin in GetNativeViewAccessible().
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       ContainsRendererAccessibilityTree) {
  LoadInitialAccessibilityTreeFromHtml(
      "<html><head><title>MyDocument</title></head>"
      "<body>Content</body></html>");

  // Get the accessibility object for the browser window.
  HWND browser_hwnd = shell()->window();
  base::win::ScopedComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindow(
      browser_hwnd,
      OBJID_WINDOW,
      IID_IAccessible,
      reinterpret_cast<void**>(browser_accessible.Receive()));
  ASSERT_EQ(S_OK, hr);

  bool found = false;
  RecursiveFindNodeInAccessibilityTree(
      browser_accessible.get(), ROLE_SYSTEM_DOCUMENT, L"MyDocument", 0, &found);
  ASSERT_EQ(found, true);
}

// Disabled because of http://crbug.com/144390.
IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       DISABLED_TestToggleButtonRoleAndStates) {
  AccessibleChecker* button_checker;
  std::string button_html("data:text/html,");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  AccessibleChecker body_checker(L"", L"body", IA2_ROLE_SECTION, L"");
  document_checker.AppendExpectedChild(&body_checker);

// Temporary macro
#define ADD_BUTTON(html, ia2_role, state) \
    button_html += html; \
    button_checker = new AccessibleChecker(L"x", ROLE_SYSTEM_PUSHBUTTON, \
      ia2_role, L""); \
    button_checker->SetExpectedState(state); \
    body_checker.AppendExpectedChild(button_checker)

  // If aria-pressed is 'undefined', empty or not present, use PUSHBUTTON
  // Otherwise use TOGGLE_BUTTON, even if the value is invalid.
  // The spec does this in an attempt future-proof in case new values are added.
  ADD_BUTTON("<span role='button' aria-pressed='false'>x</span>",
      IA2_ROLE_TOGGLE_BUTTON, 0);
  ADD_BUTTON("<span role='button' aria-pressed='true'>x</span>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_PRESSED);
  ADD_BUTTON("<span role='button' aria-pressed='mixed'>x</span>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_MIXED);
  ADD_BUTTON("<span role='button' aria-pressed='xyz'>x</span>",
    IA2_ROLE_TOGGLE_BUTTON, 0);
  ADD_BUTTON("<span role='button' aria-pressed=''>x</span>",
      ROLE_SYSTEM_PUSHBUTTON, 0);
  ADD_BUTTON("<span role='button' aria-pressed>x</span>",
      ROLE_SYSTEM_PUSHBUTTON, 0);
  ADD_BUTTON("<span role='button' aria-pressed='undefined'>x</span>",
      ROLE_SYSTEM_PUSHBUTTON, 0);
  ADD_BUTTON("<span role='button'>x</span>", ROLE_SYSTEM_PUSHBUTTON, 0);
  ADD_BUTTON("<input type='button' aria-pressed='true' value='x'/>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_PRESSED);
  ADD_BUTTON("<input type='button' aria-pressed='false' value='x'/>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<input type='button' aria-pressed='mixed' value='x'>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_MIXED);
  ADD_BUTTON("<input type='button' aria-pressed='xyz' value='x'/>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<input type='button' aria-pressed='' value='x'/>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<input type='button' aria-pressed value='x'>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<input type='button' aria-pressed='undefined' value='x'>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<input type='button' value='x'>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<button aria-pressed='true'>x</button>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_PRESSED);
  ADD_BUTTON("<button aria-pressed='false'>x</button>",
      IA2_ROLE_TOGGLE_BUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<button aria-pressed='mixed'>x</button>", IA2_ROLE_TOGGLE_BUTTON,
      STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_MIXED);
  ADD_BUTTON("<button aria-pressed='xyz'>x</button>", IA2_ROLE_TOGGLE_BUTTON,
      STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<button aria-pressed=''>x</button>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<button aria-pressed>x</button>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<button aria-pressed='undefined'>x</button>",
      ROLE_SYSTEM_PUSHBUTTON, STATE_SYSTEM_FOCUSABLE);
  ADD_BUTTON("<button>x</button>", ROLE_SYSTEM_PUSHBUTTON,
      STATE_SYSTEM_FOCUSABLE);
#undef ADD_BUTTON    // Temporary macro

  LoadInitialAccessibilityTreeFromHtml(button_html);
  document_checker.CheckAccessible(GetRendererAccessible());
}

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest,
                       SupportsISimpleDOM) {
  LoadInitialAccessibilityTreeFromHtml(
      "<body><input type='checkbox' /></body>");

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

IN_PROC_BROWSER_TEST_F(AccessibilityWinBrowserTest, TestRoleGroup) {
  LoadInitialAccessibilityTreeFromHtml(
      "<fieldset></fieldset><div role=group></div>");

  // Check the browser's copy of the renderer accessibility tree.
  AccessibleChecker grouping1_checker(L"", ROLE_SYSTEM_GROUPING, L"");
  AccessibleChecker grouping2_checker(L"", ROLE_SYSTEM_GROUPING, L"");
  AccessibleChecker document_checker(L"", ROLE_SYSTEM_DOCUMENT, L"");
  document_checker.AppendExpectedChild(&grouping1_checker);
  document_checker.AppendExpectedChild(&grouping2_checker);
  document_checker.CheckAccessible(GetRendererAccessible());
}
}  // namespace.

}  // namespace content
