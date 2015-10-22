// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"

namespace content {
namespace {


// CountedBrowserAccessibility ------------------------------------------------

// Subclass of BrowserAccessibilityWin that counts the number of instances.
class CountedBrowserAccessibility : public BrowserAccessibilityWin {
 public:
  CountedBrowserAccessibility();
  ~CountedBrowserAccessibility() override;

  static void reset() { num_instances_ = 0; }
  static int num_instances() { return num_instances_; }

 private:
  static int num_instances_;

  DISALLOW_COPY_AND_ASSIGN(CountedBrowserAccessibility);
};

// static
int CountedBrowserAccessibility::num_instances_ = 0;

CountedBrowserAccessibility::CountedBrowserAccessibility() {
  ++num_instances_;
}

CountedBrowserAccessibility::~CountedBrowserAccessibility() {
  --num_instances_;
}


// CountedBrowserAccessibilityFactory -----------------------------------------

// Factory that creates a CountedBrowserAccessibility.
class CountedBrowserAccessibilityFactory : public BrowserAccessibilityFactory {
 public:
  CountedBrowserAccessibilityFactory();

 private:
  ~CountedBrowserAccessibilityFactory() override;

  BrowserAccessibility* Create() override;

  DISALLOW_COPY_AND_ASSIGN(CountedBrowserAccessibilityFactory);
};

CountedBrowserAccessibilityFactory::CountedBrowserAccessibilityFactory() {
}

CountedBrowserAccessibilityFactory::~CountedBrowserAccessibilityFactory() {
}

BrowserAccessibility* CountedBrowserAccessibilityFactory::Create() {
  CComObject<CountedBrowserAccessibility>* instance;
  HRESULT hr = CComObject<CountedBrowserAccessibility>::CreateInstance(
      &instance);
  DCHECK(SUCCEEDED(hr));
  instance->AddRef();
  return instance;
}

}  // namespace


// BrowserAccessibilityTest ---------------------------------------------------

class BrowserAccessibilityTest : public testing::Test {
 public:
  BrowserAccessibilityTest();
  ~BrowserAccessibilityTest() override;

 private:
  void SetUp() override;

  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityTest);
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {
}

BrowserAccessibilityTest::~BrowserAccessibilityTest() {
}

void BrowserAccessibilityTest::SetUp() {
  ui::win::CreateATLModuleIfNeeded();
}


// Actual tests ---------------------------------------------------------------

// Test that BrowserAccessibilityManager correctly releases the tree of
// BrowserAccessibility instances upon delete.
TEST_F(BrowserAccessibilityTest, TestNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData button;
  button.id = 2;
  button.SetName("Button");
  button.role = ui::AX_ROLE_BUTTON;
  button.state = 0;

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = ui::AX_ROLE_CHECK_BOX;
  checkbox.state = 0;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility, and ensure that exactly 3 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, button, checkbox),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  // Delete the manager and test that all 3 instances are deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager.reset(BrowserAccessibilityManager::Create(
      MakeAXTreeUpdate(root, button, checkbox),
      NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());
  IAccessible* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  IDispatch* root_iaccessible = NULL;
  IDispatch* child1_iaccessible = NULL;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = root_accessible->get_accChild(childid_self, &root_iaccessible);
  ASSERT_EQ(S_OK, hr);
  base::win::ScopedVariant one(1);
  hr = root_accessible->get_accChild(one, &child1_iaccessible);
  ASSERT_EQ(S_OK, hr);

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  manager.reset();
  ASSERT_EQ(2, CountedBrowserAccessibility::num_instances());

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_iaccessible->Release();
  ASSERT_EQ(1, CountedBrowserAccessibility::num_instances());
  child1_iaccessible->Release();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestChildrenChange) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData text;
  text.id = 2;
  text.role = ui::AX_ROLE_STATIC_TEXT;
  text.SetName("old text");
  text.state = 0;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text),
          NULL, new CountedBrowserAccessibilityFactory()));

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  base::win::ScopedVariant one(1);
  base::win::ScopedComPtr<IDispatch> text_dispatch;
  HRESULT hr = manager->GetRoot()->ToBrowserAccessibilityWin()->get_accChild(
      one, text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedComPtr<IAccessible> text_accessible;
  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr name;
  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"old text", base::string16(name));
  name.Reset();

  text_dispatch.Release();
  text_accessible.Release();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  AXContentNodeData text2;
  text2.id = 2;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  text2.SetName("new text");
  text2.SetName("old text");
  AXEventNotificationDetails param;
  param.event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  param.update.nodes.push_back(text2);
  param.id = text2.id;
  std::vector<AXEventNotificationDetails> events;
  events.push_back(param);
  manager->OnAccessibilityEvents(events);

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = manager->GetRoot()->ToBrowserAccessibilityWin()->get_accChild(
      one, text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"new text", base::string16(name));

  text_dispatch.Release();
  text_accessible.Release();

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestChildrenChangeNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData div;
  div.id = 2;
  div.role = ui::AX_ROLE_GROUP;
  div.state = 0;

  ui::AXNodeData text3;
  text3.id = 3;
  text3.role = ui::AX_ROLE_STATIC_TEXT;
  text3.state = 0;

  ui::AXNodeData text4;
  text4.id = 4;
  text4.role = ui::AX_ROLE_STATIC_TEXT;
  text4.state = 0;

  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 0;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility and ensure that exactly 4 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, text3, text4),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(4, CountedBrowserAccessibility::num_instances());

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.child_ids.clear();
  AXEventNotificationDetails param;
  param.event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  param.update.nodes.push_back(root);
  param.id = root.id;
  std::vector<AXEventNotificationDetails> events;
  events.push_back(param);
  manager->OnAccessibilityEvents(events);
  ASSERT_EQ(1, CountedBrowserAccessibility::num_instances());

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestTextBoundaries) {
  std::string line1 = "One two three.";
  std::string line2 = "Four five six.";
  std::string text_value = line1 + '\n' + line2;

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);

  ui::AXNodeData text_field;
  text_field.id = 2;
  text_field.role = ui::AX_ROLE_TEXT_FIELD;
  text_field.AddStringAttribute(ui::AX_ATTR_VALUE, text_value);
  std::vector<int32> line_start_offsets;
  line_start_offsets.push_back(15);
  text_field.AddIntListAttribute(
      ui::AX_ATTR_LINE_BREAKS, line_start_offsets);
  text_field.child_ids.push_back(3);
  text_field.child_ids.push_back(5);
  text_field.child_ids.push_back(6);

  ui::AXNodeData static_text1;
  static_text1.id = 3;
  static_text1.role = ui::AX_ROLE_STATIC_TEXT;
  static_text1.AddStringAttribute(ui::AX_ATTR_VALUE, line1);
  static_text1.child_ids.push_back(4);

  ui::AXNodeData inline_box1;
  inline_box1.id = 4;
  inline_box1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_box1.AddStringAttribute(ui::AX_ATTR_VALUE, line1);
  std::vector<int32> word_start_offsets1;
  word_start_offsets1.push_back(0);
  word_start_offsets1.push_back(4);
  word_start_offsets1.push_back(8);
  inline_box1.AddIntListAttribute(
      ui::AX_ATTR_WORD_STARTS, word_start_offsets1);

  ui::AXNodeData line_break;
  line_break.id = 5;
  line_break.role = ui::AX_ROLE_LINE_BREAK;
  line_break.AddStringAttribute(ui::AX_ATTR_VALUE, "\n");

  ui::AXNodeData static_text2;
  static_text2.id = 6;
  static_text2.role = ui::AX_ROLE_STATIC_TEXT;
  static_text2.AddStringAttribute(ui::AX_ATTR_VALUE, line2);
  static_text2.child_ids.push_back(7);

  ui::AXNodeData inline_box2;
  inline_box2.id = 7;
  inline_box2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_box2.AddStringAttribute(ui::AX_ATTR_VALUE, line2);
  std::vector<int32> word_start_offsets2;
  word_start_offsets2.push_back(0);
  word_start_offsets2.push_back(5);
  word_start_offsets2.push_back(10);
  inline_box2.AddIntListAttribute(
      ui::AX_ATTR_WORD_STARTS, word_start_offsets2);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text_field, static_text1, inline_box1,
          line_break, static_text2, inline_box2),
          nullptr, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(7, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, root_obj);
  ASSERT_EQ(1U, root_obj->PlatformChildCount());

  BrowserAccessibilityWin* text_field_obj =
      root_obj->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, text_field_obj);

  long text_len;
  EXPECT_EQ(S_OK, text_field_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  EXPECT_EQ(S_OK, text_field_obj->get_text(0, text_len, text.Receive()));
  EXPECT_EQ(text_value, base::UTF16ToUTF8(base::string16(text)));
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(L"One ", text);
  text.Reset();

  long start;
  long end;
  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  EXPECT_EQ(1, start);
  EXPECT_EQ(2, end);
  EXPECT_STREQ(L"n", text);
  text.Reset();

  EXPECT_EQ(S_FALSE, text_field_obj->get_textAtOffset(
      text_len, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(0, end);
  EXPECT_EQ(nullptr, text);
  text.Reset();

  EXPECT_EQ(S_FALSE, text_field_obj->get_textAtOffset(
      text_len, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(0, end);
  EXPECT_EQ(nullptr, text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(4, end);
  EXPECT_STREQ(L"One ", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
      6, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  EXPECT_EQ(4, start);
  EXPECT_EQ(8, end);
  EXPECT_STREQ(L"two ", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
      text_len - 1, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  EXPECT_EQ(25, start);
  EXPECT_EQ(29, end);
  EXPECT_STREQ(L"six.", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_LINE, &start, &end, text.Receive()));
  EXPECT_EQ(0, start);
  EXPECT_EQ(15, end);
  EXPECT_STREQ(L"One two three.\n", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_textAtOffset(
      text_len, IA2_TEXT_BOUNDARY_LINE, &start, &end, text.Receive()));
  EXPECT_EQ(15, start);
  EXPECT_EQ(text_len, end);
  EXPECT_STREQ(L"Four five six.", text);
  text.Reset();

  EXPECT_EQ(S_OK, text_field_obj->get_text(
      0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  EXPECT_EQ(text_value, base::UTF16ToUTF8(base::string16(text)));

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestSimpleHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ui::AX_ROLE_STATIC_TEXT;
  text1.state = 1 << ui::AX_STATE_READ_ONLY;
  text1.SetName(text1_name);

  ui::AXNodeData text2;
  text2.id = 12;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  text2.state = 1 << ui::AX_STATE_READ_ONLY;
  text2.SetName(text2_name);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 1 << ui::AX_STATE_READ_ONLY;
  root.child_ids.push_back(11);
  root.child_ids.push_back(12);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, text1, text2),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, text.Receive()));
  EXPECT_EQ(text1_name + text2_name, base::UTF16ToUTF8(base::string16(text)));

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
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestComplexHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const std::string button1_text_name = "red";
  const std::string link1_text_name = "blue";

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ui::AX_ROLE_STATIC_TEXT;
  text1.state = 1 << ui::AX_STATE_READ_ONLY;
  text1.SetName(text1_name);

  ui::AXNodeData text2;
  text2.id = 12;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  text2.state = 1 << ui::AX_STATE_READ_ONLY;
  text2.SetName(text2_name);

  ui::AXNodeData button1, button1_text;
  button1.id = 13;
  button1_text.id = 15;
  button1_text.SetName(button1_text_name);
  button1.role = ui::AX_ROLE_BUTTON;
  button1_text.role = ui::AX_ROLE_STATIC_TEXT;
  button1.state = 1 << ui::AX_STATE_READ_ONLY;
  button1_text.state = 1 << ui::AX_STATE_READ_ONLY;
  button1.child_ids.push_back(15);

  ui::AXNodeData link1, link1_text;
  link1.id = 14;
  link1_text.id = 16;
  link1_text.SetName(link1_text_name);
  link1.role = ui::AX_ROLE_LINK;
  link1_text.role = ui::AX_ROLE_STATIC_TEXT;
  link1.state = 1 << ui::AX_STATE_READ_ONLY;
  link1_text.state = 1 << ui::AX_STATE_READ_ONLY;
  link1.child_ids.push_back(16);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = 1 << ui::AX_STATE_READ_ONLY;
  root.child_ids.push_back(11);
  root.child_ids.push_back(13);
  root.child_ids.push_back(12);
  root.child_ids.push_back(14);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root,
                           text1, button1, button1_text,
                           text2, link1, link1_text),
          NULL, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(7, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, text.Receive()));
  const std::string embed = base::UTF16ToUTF8(
      base::string16(1, BrowserAccessibilityWin::kEmbeddedCharacter));
  EXPECT_EQ(text1_name + embed + text2_name + embed,
            base::UTF16ToUTF8(base::string16(text)));
  text.Reset();

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
  EXPECT_EQ(S_OK, hypertext->get_text(0, 3, text.Receive()));
  EXPECT_STREQ(button1_text_name.c_str(),
               base::UTF16ToUTF8(base::string16(text)).c_str());
  text.Reset();
  hyperlink.Release();
  hypertext.Release();

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(1, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(link1_text_name.c_str(),
               base::UTF16ToUTF8(base::string16(text)).c_str());
  text.Reset();
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
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestCreateEmptyDocument) {
  // Try creating an empty document with busy state. Readonly is
  // set automatically.
  CountedBrowserAccessibility::reset();
  const int32 busy_state = 1 << ui::AX_STATE_BUSY;
  const int32 readonly_state = 1 << ui::AX_STATE_READ_ONLY;
  const int32 enabled_state = 1 << ui::AX_STATE_ENABLED;
  scoped_ptr<BrowserAccessibilityManager> manager(
      new BrowserAccessibilityManagerWin(
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          NULL,
          new CountedBrowserAccessibilityFactory()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetRoot();
  EXPECT_EQ(0, root->GetId());
  EXPECT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, root->GetRole());
  EXPECT_EQ(busy_state | readonly_state | enabled_state, root->GetState());

  // Tree with a child textfield.
  ui::AXNodeData tree1_1;
  tree1_1.id = 1;
  tree1_1.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree1_1.child_ids.push_back(2);

  ui::AXNodeData tree1_2;
  tree1_2.id = 2;
  tree1_2.role = ui::AX_ROLE_TEXT_FIELD;

  // Process a load complete.
  std::vector<AXEventNotificationDetails> params;
  params.push_back(AXEventNotificationDetails());
  AXEventNotificationDetails* msg = &params[0];
  msg->event_type = ui::AX_EVENT_LOAD_COMPLETE;
  msg->update.nodes.push_back(tree1_1);
  msg->update.nodes.push_back(tree1_2);
  msg->id = tree1_1.id;
  manager->OnAccessibilityEvents(params);

  // Save for later comparison.
  BrowserAccessibility* acc1_2 = manager->GetFromID(2);

  // Verify the root has changed.
  EXPECT_NE(root, manager->GetRoot());

  // And the proper child remains.
  EXPECT_EQ(ui::AX_ROLE_TEXT_FIELD, acc1_2->GetRole());
  EXPECT_EQ(2, acc1_2->GetId());

  // Tree with a child button.
  ui::AXNodeData tree2_1;
  tree2_1.id = 1;
  tree2_1.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree2_1.child_ids.push_back(3);

  ui::AXNodeData tree2_2;
  tree2_2.id = 3;
  tree2_2.role = ui::AX_ROLE_BUTTON;

  msg->update.nodes.clear();
  msg->update.nodes.push_back(tree2_1);
  msg->update.nodes.push_back(tree2_2);
  msg->id = tree2_1.id;

  // Fire another load complete.
  manager->OnAccessibilityEvents(params);

  BrowserAccessibility* acc2_2 = manager->GetFromID(3);

  // Verify the root has changed.
  EXPECT_NE(root, manager->GetRoot());

  // And the new child exists.
  EXPECT_EQ(ui::AX_ROLE_BUTTON, acc2_2->GetRole());
  EXPECT_EQ(3, acc2_2->GetId());

  // Ensure we properly cleaned up.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

// This is a regression test for a bug where the initial empty document
// loaded by a BrowserAccessibilityManagerWin couldn't be looked up by
// its UniqueIDWin, because the AX Tree was loaded in
// BrowserAccessibilityManager code before BrowserAccessibilityManagerWin
// was initialized.
TEST_F(BrowserAccessibilityTest, EmptyDocHasUniqueIdWin) {
  scoped_ptr<BrowserAccessibilityManagerWin> manager(
      new BrowserAccessibilityManagerWin(
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          NULL,
          new CountedBrowserAccessibilityFactory()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetRoot();
  EXPECT_EQ(0, root->GetId());
  EXPECT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, root->GetRole());
  EXPECT_EQ(1 << ui::AX_STATE_BUSY |
            1 << ui::AX_STATE_READ_ONLY |
            1 << ui::AX_STATE_ENABLED,
            root->GetState());

  LONG unique_id_win = root->ToBrowserAccessibilityWin()->unique_id_win();
  ASSERT_EQ(root, manager->GetFromUniqueIdWin(unique_id_win));
}

TEST_F(BrowserAccessibilityTest, TestIA2Attributes) {
  ui::AXNodeData pseudo_before;
  pseudo_before.id = 2;
  pseudo_before.role = ui::AX_ROLE_DIV;
  pseudo_before.AddStringAttribute(ui::AX_ATTR_HTML_TAG, "<pseudo:before>");
  pseudo_before.AddStringAttribute(ui::AX_ATTR_DISPLAY, "none");

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = ui::AX_ROLE_CHECK_BOX;
  checkbox.state = 1 << ui::AX_STATE_CHECKED;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = (1 << ui::AX_STATE_READ_ONLY) | (1 << ui::AX_STATE_FOCUSABLE);
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, pseudo_before, checkbox), nullptr,
          new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
      ASSERT_NE(nullptr, root_accessible);
      ASSERT_EQ(2U, root_accessible->PlatformChildCount());

      BrowserAccessibilityWin* pseudo_accessible =
          root_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
      ASSERT_NE(nullptr, pseudo_accessible);

  base::win::ScopedBstr attributes;
  HRESULT hr = pseudo_accessible->get_attributes(attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(nullptr, static_cast<BSTR>(attributes));
  std::wstring attributes_str(attributes, attributes.Length());
  EXPECT_EQ(L"display:none;tag:<pseudo\\:before>;", attributes_str);

  BrowserAccessibilityWin* checkbox_accessible =
      root_accessible->PlatformGetChild(1)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, checkbox_accessible);

  attributes.Reset();
  hr = checkbox_accessible->get_attributes(attributes.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_NE(nullptr, static_cast<BSTR>(attributes));
  attributes_str = std::wstring(attributes, attributes.Length());
  EXPECT_EQ(L"checkable:true;", attributes_str);

  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

/**
 * Ensures that ui::AX_ATTR_TEXT_SEL_START/END attributes are correctly used to
 * determine caret position and text selection in simple form fields.
 */
TEST_F(BrowserAccessibilityTest, TestCaretAndSelectionInSimpleFields) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = (1 << ui::AX_STATE_READ_ONLY) | (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData combo_box;
  combo_box.id = 2;
  combo_box.role = ui::AX_ROLE_COMBO_BOX;
  combo_box.state = (1 << ui::AX_STATE_EDITABLE) |
      (1 << ui::AX_STATE_FOCUSABLE) | (1 << ui::AX_STATE_FOCUSED);
  combo_box.SetValue("Test1");
  // Place the caret between 't' and 'e'.
  combo_box.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, 1);
  combo_box.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, 1);

  ui::AXNodeData text_field;
  text_field.id = 3;
  text_field.role = ui::AX_ROLE_TEXT_FIELD;
  text_field.state = (1 << ui::AX_STATE_EDITABLE) |
      (1 << ui::AX_STATE_FOCUSABLE);
  text_field.SetValue("Test2");
  // Select the letter 'e'.
  text_field.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_START, 1);
  text_field.AddIntAttribute(ui::AX_ATTR_TEXT_SEL_END, 2);

  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, combo_box, text_field),
          nullptr, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
      ASSERT_NE(nullptr, root_accessible);
      ASSERT_EQ(2U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* combo_box_accessible =
      root_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, combo_box_accessible);
  manager->SetFocus(combo_box_accessible, false /* notify */);
  ASSERT_EQ(combo_box_accessible,
      manager->GetFocus(root_accessible)->ToBrowserAccessibilityWin());
  BrowserAccessibilityWin* text_field_accessible =
      root_accessible->PlatformGetChild(1)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, text_field_accessible);

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;
  LONG selection_start = -2;
  LONG selection_end = -2;

  // Test get_caretOffset.
  HRESULT hr = combo_box_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, caret_offset);
  // The caret should be at the end of the selection.
  hr = text_field_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2L, caret_offset);

  // Move the focus to the text field.
  combo_box.state &= ~(1 << ui::AX_STATE_FOCUSED);
  text_field.state |= 1 << ui::AX_STATE_FOCUSED;
  manager->SetFocus(text_field_accessible, false /* notify */);
  ASSERT_EQ(text_field_accessible,
      manager->GetFocus(root_accessible)->ToBrowserAccessibilityWin());

  // The caret should not have moved.
  hr = text_field_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(2L, caret_offset);

  // Test get_nSelections.
  hr = combo_box_accessible->get_nSelections(&n_selections);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, n_selections);
  hr = text_field_accessible->get_nSelections(&n_selections);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, n_selections);

  // Test get_selection.
  hr = combo_box_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(E_INVALIDARG, hr); // No selections available.
  // Invalid in_args should not modify out_args.
  EXPECT_EQ(-2L, selection_start);
  EXPECT_EQ(-2L, selection_end);
  hr = text_field_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, selection_start);
  EXPECT_EQ(2L, selection_end);

  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestCaretInContentEditables) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = (1 << ui::AX_STATE_READ_ONLY) | (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ui::AX_ROLE_DIV;
  div_editable.state = (1 << ui::AX_STATE_EDITABLE) |
      (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData text;
  text.id = 3;
  text.role = ui::AX_ROLE_STATIC_TEXT;
  text.state = (1 << ui::AX_STATE_EDITABLE);
  text.SetName("Click ");

  ui::AXNodeData link;
  link.id = 4;
  link.role = ui::AX_ROLE_LINK;
  link.state = (1 << ui::AX_STATE_EDITABLE) |
      (1 << ui::AX_STATE_FOCUSABLE) | (1 << ui::AX_STATE_LINKED);
  link.SetName("here");

  ui::AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ui::AX_ROLE_STATIC_TEXT;
  link_text.state = (1 << ui::AX_STATE_EDITABLE) |
      (1 << ui::AX_STATE_FOCUSABLE) | (1 << ui::AX_STATE_LINKED);
  link_text.SetName("here");

  root.child_ids.push_back(2);
  div_editable.child_ids.push_back(3);
  div_editable.child_ids.push_back(4);
  link.child_ids.push_back(5);

  ui::AXTreeUpdate update = MakeAXTreeUpdate(
      root, div_editable, link, link_text, text);

  // Place the caret between 'h' and 'e'.
  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = 5;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_focus_offset = 1;

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update,
          nullptr, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(5, CountedBrowserAccessibility::num_instances());

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
      ASSERT_NE(nullptr, root_accessible);
      ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_editable_accessible =
      root_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, div_editable_accessible);
  ASSERT_EQ(2U, div_editable_accessible->PlatformChildCount());

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;

  // No selection should be present.
  HRESULT hr = div_editable_accessible->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, n_selections);

  // The caret should be on the embedded object character.
  hr = div_editable_accessible->get_caretOffset(&caret_offset);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(6L, caret_offset);

  // Move the focus to the content editable.
  div_editable.state |= 1 << ui::AX_STATE_FOCUSED;
  manager->SetFocus(div_editable_accessible, false /* notify */);
  ASSERT_EQ(div_editable_accessible,
      manager->GetFocus(root_accessible)->ToBrowserAccessibilityWin());

  BrowserAccessibilityWin* text_accessible =
      div_editable_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      div_editable_accessible->PlatformGetChild(1)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, link_accessible);
  ASSERT_EQ(1U, link_accessible->PlatformChildCount());

  BrowserAccessibilityWin* link_text_accessible =
      link_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, link_text_accessible);

  // The caret should not have moved.
  hr = div_editable_accessible->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, n_selections);
  hr = div_editable_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(6L, caret_offset);

  hr = link_accessible->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, n_selections);
  hr = link_text_accessible->get_nSelections(&n_selections);
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, n_selections);

  hr = link_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, caret_offset);
  hr = link_text_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, caret_offset);

  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestSelectionInContentEditables) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = (1 << ui::AX_STATE_READ_ONLY) | (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData div_editable;
  div_editable.id = 2;
  div_editable.role = ui::AX_ROLE_DIV;
  div_editable.state = (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData text;
  text.id = 3;
  text.role = ui::AX_ROLE_STATIC_TEXT;
  text.SetName("Click ");

  ui::AXNodeData link;
  link.id = 4;
  link.role = ui::AX_ROLE_LINK;
  link.state = (1 << ui::AX_STATE_FOCUSABLE) | (1 << ui::AX_STATE_LINKED);
  link.SetName("here");

  ui::AXNodeData link_text;
  link_text.id = 5;
  link_text.role = ui::AX_ROLE_STATIC_TEXT;
  link_text.state = (1 << ui::AX_STATE_FOCUSABLE) | (1 << ui::AX_STATE_LINKED);
  link_text.SetName("here");

  root.child_ids.push_back(2);
  div_editable.child_ids.push_back(3);
  div_editable.child_ids.push_back(4);
  link.child_ids.push_back(5);

  ui::AXTreeUpdate update =
      MakeAXTreeUpdate(root, div_editable, link, link_text, text);

  // Select the following part of the text: "lick here".
  update.has_tree_data = true;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_focus_offset = 4;

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          update,
          nullptr, new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(5, CountedBrowserAccessibility::num_instances());

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
      ASSERT_NE(nullptr, root_accessible);
      ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_editable_accessible =
      root_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, div_editable_accessible);
  ASSERT_EQ(2U, div_editable_accessible->PlatformChildCount());

  // -2 is never a valid offset.
  LONG caret_offset = -2;
  LONG n_selections = -2;
  LONG selection_start = -2;
  LONG selection_end = -2;

  BrowserAccessibilityWin* text_accessible =
      div_editable_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      div_editable_accessible->PlatformGetChild(1)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, link_accessible);
  ASSERT_EQ(1U, link_accessible->PlatformChildCount());

  BrowserAccessibilityWin* link_text_accessible =
      link_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, link_text_accessible);

  // get_nSelections should work on all objects.
  HRESULT hr = div_editable_accessible->get_nSelections(&n_selections);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, n_selections);
  hr = text_accessible->get_nSelections(&n_selections);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, n_selections);
  hr = link_accessible->get_nSelections(&n_selections);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, n_selections);
  hr = link_text_accessible->get_nSelections(&n_selections);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, n_selections);

  // get_selection should be unaffected by focus placement.
  hr = div_editable_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, selection_start);
  // selection_end should be after embedded object character.
  EXPECT_EQ(7L, selection_end);

  hr = text_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, selection_start);
  // No embedded character on this object, only the first part of the text.
  EXPECT_EQ(6L, selection_end);
  hr = link_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, selection_start);
  EXPECT_EQ(4L, selection_end);
  hr = link_text_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(0L, selection_start);
  EXPECT_EQ(4L, selection_end);

  // The caret should be at the focus (the end) of the selection.
  hr = div_editable_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(7L, caret_offset);

  // Move the focus to the content editable.
  div_editable.state |= 1 << ui::AX_STATE_FOCUSED;
  manager->SetFocus(div_editable_accessible, false /* notify */);
  ASSERT_EQ(div_editable_accessible,
      manager->GetFocus(root_accessible)->ToBrowserAccessibilityWin());

  // The caret should not have moved.
  hr = div_editable_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(7L, caret_offset);

  // The caret offset should reflect the position of the selection's focus in
  // any given object.
  hr = link_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(4L, caret_offset);
  hr = link_text_accessible->get_caretOffset(&caret_offset);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(4L, caret_offset);

  hr = div_editable_accessible->get_selection(
      0L /* selection_index */, &selection_start, &selection_end);;
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(1L, selection_start);
  EXPECT_EQ(7L, selection_end);

  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestIAccessibleHyperlink) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.state = (1 << ui::AX_STATE_READ_ONLY) | (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData div;
  div.id = 2;
  div.role = ui::AX_ROLE_DIV;
  div.state = (1 << ui::AX_STATE_FOCUSABLE);

  ui::AXNodeData text;
  text.id = 3;
  text.role = ui::AX_ROLE_STATIC_TEXT;
  text.SetName("Click ");

  ui::AXNodeData link;
  link.id = 4;
  link.role = ui::AX_ROLE_LINK;
  link.state = (1 << ui::AX_STATE_FOCUSABLE) | (1 << ui::AX_STATE_LINKED);
  link.SetName("here");
  link.AddStringAttribute(ui::AX_ATTR_URL, "example.com");

  root.child_ids.push_back(2);
  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, link, text), nullptr,
          new CountedBrowserAccessibilityFactory()));
  ASSERT_EQ(4, CountedBrowserAccessibility::num_instances());

  ASSERT_NE(nullptr, manager->GetRoot());
  BrowserAccessibilityWin* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(1U, root_accessible->PlatformChildCount());

  BrowserAccessibilityWin* div_accessible =
      root_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, div_accessible);
  ASSERT_EQ(2U, div_accessible->PlatformChildCount());

  BrowserAccessibilityWin* text_accessible =
      div_accessible->PlatformGetChild(0)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, text_accessible);
  BrowserAccessibilityWin* link_accessible =
      div_accessible->PlatformGetChild(1)->ToBrowserAccessibilityWin();
  ASSERT_NE(nullptr, link_accessible);

  // -1 is never a valid value.
  LONG n_actions = -1;
  LONG start_index = -1;
  LONG end_index = -1;

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  base::win::ScopedVariant anchor;
  base::win::ScopedVariant anchor_target;
  base::win::ScopedBstr bstr;

  base::string16 div_hypertext(L"Click ");
  div_hypertext.push_back(BrowserAccessibilityWin::kEmbeddedCharacter);

  // div_accessible and link_accessible are the only IA2 hyperlinks.
  EXPECT_HRESULT_FAILED(root_accessible->QueryInterface(
      IID_IAccessibleHyperlink, reinterpret_cast<void**>(hyperlink.Receive())));
  hyperlink.Release();
  EXPECT_HRESULT_SUCCEEDED(div_accessible->QueryInterface(
      IID_IAccessibleHyperlink, reinterpret_cast<void**>(hyperlink.Receive())));
  hyperlink.Release();
  EXPECT_HRESULT_FAILED(text_accessible->QueryInterface(
      IID_IAccessibleHyperlink, reinterpret_cast<void**>(hyperlink.Receive())));
  hyperlink.Release();
  EXPECT_HRESULT_SUCCEEDED(link_accessible->QueryInterface(
      IID_IAccessibleHyperlink, reinterpret_cast<void**>(hyperlink.Receive())));
  hyperlink.Release();

  EXPECT_HRESULT_SUCCEEDED(root_accessible->nActions(&n_actions));
  EXPECT_EQ(0, n_actions);
  EXPECT_HRESULT_SUCCEEDED(div_accessible->nActions(&n_actions));
  EXPECT_EQ(1, n_actions);
  EXPECT_HRESULT_SUCCEEDED(text_accessible->nActions(&n_actions));
  EXPECT_EQ(0, n_actions);
  EXPECT_HRESULT_SUCCEEDED(link_accessible->nActions(&n_actions));
  EXPECT_EQ(1, n_actions);

  EXPECT_HRESULT_FAILED(root_accessible->get_anchor(0, anchor.Receive()));
  anchor.Reset();
  HRESULT hr = div_accessible->get_anchor(0, anchor.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor.type());
  bstr.Reset(V_BSTR(anchor.ptr()));
  EXPECT_STREQ(div_hypertext.c_str(), bstr);
  bstr.Reset();
  anchor.Reset();
  EXPECT_HRESULT_FAILED(text_accessible->get_anchor(0, anchor.Receive()));
  anchor.Reset();
  hr = link_accessible->get_anchor(0, anchor.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor.type());
  bstr.Reset(V_BSTR(anchor.ptr()));
  EXPECT_STREQ(L"here", bstr);
  bstr.Reset();
  anchor.Reset();
  EXPECT_HRESULT_FAILED(div_accessible->get_anchor(1, anchor.Receive()));
  anchor.Reset();
  EXPECT_HRESULT_FAILED(link_accessible->get_anchor(1, anchor.Receive()));
  anchor.Reset();

  EXPECT_HRESULT_FAILED(
      root_accessible->get_anchorTarget(0, anchor_target.Receive()));
  anchor_target.Reset();
  hr = div_accessible->get_anchorTarget(0, anchor_target.Receive());
  EXPECT_EQ(S_FALSE, hr);
  EXPECT_EQ(VT_BSTR, anchor_target.type());
  bstr.Reset(V_BSTR(anchor_target.ptr()));
  // Target should be empty.
  EXPECT_STREQ(L"", bstr);
  bstr.Reset();
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      text_accessible->get_anchorTarget(0, anchor_target.Receive()));
  anchor_target.Reset();
  hr = link_accessible->get_anchorTarget(0, anchor_target.Receive());
  EXPECT_EQ(S_OK, hr);
  EXPECT_EQ(VT_BSTR, anchor_target.type());
  bstr.Reset(V_BSTR(anchor_target.ptr()));
  EXPECT_STREQ(L"example.com", bstr);
  bstr.Reset();
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      div_accessible->get_anchorTarget(1, anchor_target.Receive()));
  anchor_target.Reset();
  EXPECT_HRESULT_FAILED(
      link_accessible->get_anchorTarget(1, anchor_target.Receive()));
  anchor_target.Reset();

  EXPECT_HRESULT_FAILED(root_accessible->get_startIndex(&start_index));
  EXPECT_HRESULT_SUCCEEDED(div_accessible->get_startIndex(&start_index));
  EXPECT_EQ(0, start_index);
  EXPECT_HRESULT_FAILED(text_accessible->get_startIndex(&start_index));
  EXPECT_HRESULT_SUCCEEDED(link_accessible->get_startIndex(&start_index));
  EXPECT_EQ(6, start_index);

  EXPECT_HRESULT_FAILED(root_accessible->get_endIndex(&end_index));
  EXPECT_HRESULT_SUCCEEDED(div_accessible->get_endIndex(&end_index));
  EXPECT_EQ(1, end_index);
  EXPECT_HRESULT_FAILED(text_accessible->get_endIndex(&end_index));
  EXPECT_HRESULT_SUCCEEDED(link_accessible->get_endIndex(&end_index));
  EXPECT_EQ(7, end_index);
}

TEST_F(BrowserAccessibilityTest, TestPlatformDeepestFirstLastChild) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;

  ui::AXNodeData child1;
  child1.id = 2;
  root.child_ids.push_back(2);

  ui::AXNodeData child2;
  child2.id = 3;
  root.child_ids.push_back(3);

  ui::AXNodeData child2_child1;
  child2_child1.id = 4;
  child2.child_ids.push_back(4);

  ui::AXNodeData child2_child2;
  child2_child2.id = 5;
  child2.child_ids.push_back(5);

  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, child1, child2, child2_child1, child2_child2),
          nullptr, new CountedBrowserAccessibilityFactory()));

  auto root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());
  auto child1_accessible = root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, child1_accessible);
  auto child2_accessible = root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, child2_accessible);
  ASSERT_EQ(2U, child2_accessible->PlatformChildCount());
  auto child2_child1_accessible = child2_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, child2_child1_accessible);
  auto child2_child2_accessible = child2_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, child2_child2_accessible);

  EXPECT_EQ(child1_accessible, root_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(child2_child2_accessible,
            root_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child1_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child1_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(child2_child1_accessible,
            child2_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(child2_child2_accessible,
            child2_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child1_accessible->PlatformDeepestLastChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->PlatformDeepestFirstChild());
  EXPECT_EQ(nullptr, child2_child2_accessible->PlatformDeepestLastChild());
}

TEST_F(BrowserAccessibilityTest, TestSanitizeStringAttributeForIA2) {
  base::string16 input(L"\\:=,;");
  base::string16 output;
  BrowserAccessibilityWin::SanitizeStringAttributeForIA2(input, &output);
  EXPECT_EQ(L"\\\\\\:\\=\\,\\;", output);
}

}  // namespace content
