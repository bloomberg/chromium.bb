// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/scoped_comptr_win.h"
#include "chrome/browser/browser_accessibility_manager_win.h"
#include "chrome/browser/browser_accessibility_win.h"
#include "testing/gtest/include/gtest/gtest.h"

using webkit_glue::WebAccessibility;

// Subclass of BrowserAccessibility that counts the number of instances.
class CountedBrowserAccessibility : public BrowserAccessibility {
 public:
  CountedBrowserAccessibility() { global_obj_count_++; }
  virtual ~CountedBrowserAccessibility() { global_obj_count_--; }
  static int global_obj_count_;
};

int CountedBrowserAccessibility::global_obj_count_ = 0;

// Factory that creates a CountedBrowserAccessibility.
class CountedBrowserAccessibilityFactory : public BrowserAccessibilityFactory {
 public:
  virtual ~CountedBrowserAccessibilityFactory() {}
  virtual BrowserAccessibility* Create() {
    CComObject<CountedBrowserAccessibility>* instance;
    HRESULT hr = CComObject<CountedBrowserAccessibility>::CreateInstance(
        &instance);
    DCHECK(SUCCEEDED(hr));
    return instance->NewReference();
  }
};

VARIANT CreateI4Variant(LONG value) {
  VARIANT variant = {0};

  V_VT(&variant) = VT_I4;
  V_I4(&variant) = value;

  return variant;
}

class BrowserAccessibilityTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // ATL needs a pointer to a COM module.
    static CComModule module;
    _pAtlModule = &module;

    // Make sure COM is initialized for this thread; it's safe to call twice.
    ::CoInitialize(NULL);
  }

  virtual void TearDown()
  {
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
      new BrowserAccessibilityManager(
          GetDesktopWindow(), root, NULL,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and test that all 3 instances are deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager = new BrowserAccessibilityManager(
      GetDesktopWindow(), root, NULL,
      new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);
  BrowserAccessibility* root_accessible = manager->GetRoot();
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
  text.value = L"old text";
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
      new BrowserAccessibilityManager(
          GetDesktopWindow(), root, NULL,
          new CountedBrowserAccessibilityFactory());

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  ScopedComPtr<IDispatch> text_dispatch;
  HRESULT hr = manager->GetRoot()->get_accChild(CreateI4Variant(1),
                                                text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  ScopedComPtr<IAccessible> text_accessible;
  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  CComBSTR value;
  hr = text_accessible->get_accValue(CreateI4Variant(CHILDID_SELF), &value);
  ASSERT_EQ(S_OK, hr);
  EXPECT_STREQ(L"old text", value.m_str);

  text_dispatch.Release();
  text_accessible.Release();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  text.value = L"new text";
  std::vector<WebAccessibility> acc_changes;
  acc_changes.push_back(text);
  manager->OnAccessibilityObjectChildrenChange(acc_changes);

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = manager->GetRoot()->get_accChild(
      CreateI4Variant(1),
      text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accValue(CreateI4Variant(CHILDID_SELF), &value);
  ASSERT_EQ(S_OK, hr);
  EXPECT_STREQ(L"new text", value.m_str);

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
      new BrowserAccessibilityManager(
          GetDesktopWindow(), root, NULL,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(4, CountedBrowserAccessibility::global_obj_count_);

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.children.clear();
  std::vector<WebAccessibility> acc_changes;
  acc_changes.push_back(root);
  manager->OnAccessibilityObjectChildrenChange(acc_changes);
  ASSERT_EQ(1, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}
