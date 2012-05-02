// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_comptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/common/accessibility_messages.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"

using webkit_glue::WebAccessibility;

namespace {

// Subclass of BrowserAccessibilityWin that counts the number of instances.
class CountedBrowserAccessibility : public BrowserAccessibilityWin {
 public:
  CountedBrowserAccessibility() { global_obj_count_++; }
  virtual ~CountedBrowserAccessibility() { global_obj_count_--; }
  static int global_obj_count_;
};

int CountedBrowserAccessibility::global_obj_count_ = 0;

// Factory that creates a CountedBrowserAccessibility.
class CountedBrowserAccessibilityFactory
    : public BrowserAccessibilityFactory {
 public:
  virtual ~CountedBrowserAccessibilityFactory() {}
  virtual BrowserAccessibility* Create() {
    CComObject<CountedBrowserAccessibility>* instance;
    HRESULT hr = CComObject<CountedBrowserAccessibility>::CreateInstance(
        &instance);
    DCHECK(SUCCEEDED(hr));
    instance->AddRef();
    return instance;
  }
};

}  // anonymous namespace

VARIANT CreateI4Variant(LONG value) {
  VARIANT variant = {0};

  V_VT(&variant) = VT_I4;
  V_I4(&variant) = value;

  return variant;
}

class BrowserAccessibilityTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ui::win::CreateATLModuleIfNeeded();
    ::CoInitialize(NULL);
  }

  virtual void TearDown() {
    ::CoUninitialize();
  }
};

// Test that BrowserAccessibilityManager correctly releases the tree of
// BrowserAccessibility instances upon delete.
TEST_F(BrowserAccessibilityTest, TestNoLeaks) {
  // Create WebAccessibility objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  WebAccessibility button;
  button.id = 2;
  button.name = L"Button";
  button.role = WebAccessibility::ROLE_BUTTON;
  button.state = 0;

  WebAccessibility checkbox;
  checkbox.id = 3;
  checkbox.name = L"Checkbox";
  checkbox.role = WebAccessibility::ROLE_CHECKBOX;
  checkbox.state = 0;

  WebAccessibility root;
  root.id = 1;
  root.name = L"Document";
  root.role = WebAccessibility::ROLE_DOCUMENT;
  root.state = 0;
  root.children.push_back(button);
  root.children.push_back(checkbox);

  // Construct a BrowserAccessibilityManager with this WebAccessibility tree
  // and a factory for an instance-counting BrowserAccessibility, and ensure
  // that exactly 3 instances were created. Note that the manager takes
  // ownership of the factory.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          GetDesktopWindow(),
          root,
          NULL,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and test that all 3 instances are deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager =
      BrowserAccessibilityManager::Create(
          GetDesktopWindow(),
          root,
          NULL,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);
  IAccessible* root_accessible =
      manager->GetRoot()->toBrowserAccessibilityWin();
  IDispatch* root_iaccessible = NULL;
  IDispatch* child1_iaccessible = NULL;
  VARIANT var_child;
  var_child.vt = VT_I4;
  var_child.lVal = CHILDID_SELF;
  HRESULT hr = root_accessible->get_accChild(var_child, &root_iaccessible);
  ASSERT_EQ(S_OK, hr);
  var_child.lVal = 1;
  hr = root_accessible->get_accChild(var_child, &child1_iaccessible);
  ASSERT_EQ(S_OK, hr);

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  delete manager;
  ASSERT_EQ(2, CountedBrowserAccessibility::global_obj_count_);

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_iaccessible->Release();
  ASSERT_EQ(1, CountedBrowserAccessibility::global_obj_count_);
  child1_iaccessible->Release();
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST_F(BrowserAccessibilityTest, TestChildrenChange) {
  // Create WebAccessibility objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  WebAccessibility text;
  text.id = 2;
  text.role = WebAccessibility::ROLE_STATIC_TEXT;
  text.name = L"old text";
  text.state = 0;

  WebAccessibility root;
  root.id = 1;
  root.name = L"Document";
  root.role = WebAccessibility::ROLE_DOCUMENT;
  root.state = 0;
  root.children.push_back(text);

  // Construct a BrowserAccessibilityManager with this WebAccessibility tree
  // and a factory for an instance-counting BrowserAccessibility.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          GetDesktopWindow(),
          root,
          NULL,
          new CountedBrowserAccessibilityFactory());

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  base::win::ScopedComPtr<IDispatch> text_dispatch;
  HRESULT hr = manager->GetRoot()->toBrowserAccessibilityWin()->get_accChild(
      CreateI4Variant(1), text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedComPtr<IAccessible> text_accessible;
  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  CComBSTR name;
  hr = text_accessible->get_accName(CreateI4Variant(CHILDID_SELF), &name);
  ASSERT_EQ(S_OK, hr);
  EXPECT_STREQ(L"old text", name.m_str);

  text_dispatch.Release();
  text_accessible.Release();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  text.name = L"new text";
  AccessibilityHostMsg_NotificationParams param;
  param.notification_type = AccessibilityNotificationChildrenChanged;
  param.acc_tree = text;
  param.includes_children = true;
  param.id = text.id;
  std::vector<AccessibilityHostMsg_NotificationParams> notifications;
  notifications.push_back(param);
  manager->OnAccessibilityNotifications(notifications);

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = manager->GetRoot()->toBrowserAccessibilityWin()->get_accChild(
      CreateI4Variant(1),
      text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accName(CreateI4Variant(CHILDID_SELF), &name);
  ASSERT_EQ(S_OK, hr);
  EXPECT_STREQ(L"new text", name.m_str);

  text_dispatch.Release();
  text_accessible.Release();

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST_F(BrowserAccessibilityTest, TestChildrenChangeNoLeaks) {
  // Create WebAccessibility objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  WebAccessibility text;
  text.id = 3;
  text.role = WebAccessibility::ROLE_STATIC_TEXT;
  text.state = 0;

  WebAccessibility div;
  div.id = 2;
  div.role = WebAccessibility::ROLE_GROUP;
  div.state = 0;

  div.children.push_back(text);
  text.id = 4;
  div.children.push_back(text);

  WebAccessibility root;
  root.id = 1;
  root.role = WebAccessibility::ROLE_DOCUMENT;
  root.state = 0;
  root.children.push_back(div);

  // Construct a BrowserAccessibilityManager with this WebAccessibility tree
  // and a factory for an instance-counting BrowserAccessibility and ensure
  // that exactly 4 instances were created. Note that the manager takes
  // ownership of the factory.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          GetDesktopWindow(),
          root,
          NULL,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(4, CountedBrowserAccessibility::global_obj_count_);

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.children.clear();
  AccessibilityHostMsg_NotificationParams param;
  param.notification_type = AccessibilityNotificationChildrenChanged;
  param.acc_tree = root;
  param.includes_children = true;
  param.id = root.id;
  std::vector<AccessibilityHostMsg_NotificationParams> notifications;
  notifications.push_back(param);
  manager->OnAccessibilityNotifications(notifications);
  ASSERT_EQ(1, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST_F(BrowserAccessibilityTest, TestTextBoundaries) {
  WebAccessibility text1;
  text1.id = 11;
  text1.role = WebAccessibility::ROLE_TEXT_FIELD;
  text1.state = 0;
  text1.value = L"One two three.\nFour five six.";
  text1.line_breaks.push_back(15);

  WebAccessibility root;
  root.id = 1;
  root.role = WebAccessibility::ROLE_DOCUMENT;
  root.state = 0;
  root.children.push_back(text1);

  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager = BrowserAccessibilityManager::Create(
      GetDesktopWindow(), root, NULL,
      new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(2, CountedBrowserAccessibility::global_obj_count_);

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->toBrowserAccessibilityWin();
  BrowserAccessibilityWin* text1_obj =
      root_obj->GetChild(0)->toBrowserAccessibilityWin();

  BSTR text;
  long start;
  long end;

  long text1_len;
  ASSERT_EQ(S_OK, text1_obj->get_nCharacters(&text1_len));

  ASSERT_EQ(S_OK, text1_obj->get_text(0, text1_len, &text));
  ASSERT_EQ(text, text1.value);
  SysFreeString(text);

  ASSERT_EQ(S_OK, text1_obj->get_text(0, 4, &text));
  ASSERT_EQ(text, string16(L"One "));
  SysFreeString(text);

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_CHAR, &start, &end, &text));
  ASSERT_EQ(start, 1);
  ASSERT_EQ(end, 2);
  ASSERT_EQ(text, string16(L"n"));
  SysFreeString(text);

  ASSERT_EQ(S_FALSE, text1_obj->get_textAtOffset(
      text1_len, IA2_TEXT_BOUNDARY_CHAR, &start, &end, &text));
  ASSERT_EQ(start, text1_len);
  ASSERT_EQ(end, text1_len);

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_WORD, &start, &end, &text));
  ASSERT_EQ(start, 0);
  ASSERT_EQ(end, 3);
  ASSERT_EQ(text, string16(L"One"));
  SysFreeString(text);

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      6, IA2_TEXT_BOUNDARY_WORD, &start, &end, &text));
  ASSERT_EQ(start, 4);
  ASSERT_EQ(end, 7);
  ASSERT_EQ(text, string16(L"two"));
  SysFreeString(text);

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      text1_len, IA2_TEXT_BOUNDARY_WORD, &start, &end, &text));
  ASSERT_EQ(start, 25);
  ASSERT_EQ(end, 29);
  ASSERT_EQ(text, string16(L"six."));
  SysFreeString(text);

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_LINE, &start, &end, &text));
  ASSERT_EQ(start, 0);
  ASSERT_EQ(end, 15);
  ASSERT_EQ(text, string16(L"One two three.\n"));
  SysFreeString(text);

  ASSERT_EQ(S_OK, text1_obj->get_text(0, IA2_TEXT_OFFSET_LENGTH, &text));
  ASSERT_EQ(text, string16(L"One two three.\nFour five six."));
  SysFreeString(text);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST_F(BrowserAccessibilityTest, TestSimpleHypertext) {
  WebAccessibility text1;
  text1.id = 11;
  text1.role = WebAccessibility::ROLE_STATIC_TEXT;
  text1.state = 0;
  text1.name = L"One two three.";

  WebAccessibility text2;
  text2.id = 12;
  text2.role = WebAccessibility::ROLE_STATIC_TEXT;
  text2.state = 0;
  text2.name = L" Four five six.";

  WebAccessibility root;
  root.id = 1;
  root.role = WebAccessibility::ROLE_DOCUMENT;
  root.state = 0;
  root.children.push_back(text1);
  root.children.push_back(text2);

  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager = BrowserAccessibilityManager::Create(
      GetDesktopWindow(), root, NULL,
      new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->toBrowserAccessibilityWin();

  BSTR text;

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, &text));
  EXPECT_EQ(text, text1.name + text2.name);
  SysFreeString(text);

  long hyperlink_count;
  ASSERT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(0, hyperlink_count);

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(0, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(28, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(29, hyperlink.Receive()));

  long hyperlink_index;
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(28, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(-1, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(29, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST_F(BrowserAccessibilityTest, TestComplexHypertext) {
  WebAccessibility text1;
  text1.id = 11;
  text1.role = WebAccessibility::ROLE_STATIC_TEXT;
  text1.state = 0;
  text1.name = L"One two three.";

  WebAccessibility text2;
  text2.id = 12;
  text2.role = WebAccessibility::ROLE_STATIC_TEXT;
  text2.state = 0;
  text2.name = L" Four five six.";

  WebAccessibility button1, button1_text;
  button1.id = 13;
  button1_text.id = 15;
  button1_text.name = L"red";
  button1.role = WebAccessibility::ROLE_BUTTON;
  button1_text.role = WebAccessibility::ROLE_STATIC_TEXT;
  button1.state = 0;
  button1.children.push_back(button1_text);

  WebAccessibility link1, link1_text;
  link1.id = 14;
  link1_text.id = 16;
  link1_text.name = L"blue";
  link1.role = WebAccessibility::ROLE_LINK;
  link1_text.role = WebAccessibility::ROLE_STATIC_TEXT;
  link1.state = 0;
  link1.children.push_back(link1_text);

  WebAccessibility root;
  root.id = 1;
  root.role = WebAccessibility::ROLE_DOCUMENT;
  root.state = 0;
  root.children.push_back(text1);
  root.children.push_back(button1);
  root.children.push_back(text2);
  root.children.push_back(link1);

  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager = BrowserAccessibilityManager::Create(
      GetDesktopWindow(), root, NULL,
      new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(7, CountedBrowserAccessibility::global_obj_count_);

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->toBrowserAccessibilityWin();

  BSTR text;

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, &text));
  const string16 embed = BrowserAccessibilityWin::kEmbeddedCharacter;
  EXPECT_EQ(text, text1.name + embed + text2.name + embed);
  SysFreeString(text);

  long hyperlink_count;
  ASSERT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(2, hyperlink_count);

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  base::win::ScopedComPtr<IAccessibleText> hypertext;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(2, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(28, hyperlink.Receive()));

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(0, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 3, &text));
  EXPECT_EQ(text, string16(L"red"));
  SysFreeString(text);
  hyperlink.Release();
  hypertext.Release();

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(1, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 4, &text));
  EXPECT_EQ(text, string16(L"blue"));
  SysFreeString(text);
  hyperlink.Release();
  hypertext.Release();

  long hyperlink_index;
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(28, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(14, &hyperlink_index));
  EXPECT_EQ(0, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(30, &hyperlink_index));
  EXPECT_EQ(1, hyperlink_index);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}
