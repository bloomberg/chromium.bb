// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/virtual_keyboard_manager_handler.h"

#include <map>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef std::multimap<
  std::string, const chromeos::input_method::VirtualKeyboard*> LayoutToKeyboard;
typedef std::map<
  GURL, const chromeos::input_method::VirtualKeyboard*> UrlToKeyboard;

template <size_t L>
std::set<std::string> CreateLayoutSet(const char* (&layouts)[L]) {
  return std::set<std::string>(layouts, layouts + L);
}

}  // namespace

namespace chromeos {

class Testee : public VirtualKeyboardManagerHandler {
 public:
  // Change access rights.
  using VirtualKeyboardManagerHandler::ValidateUrl;
  using VirtualKeyboardManagerHandler::CreateVirtualKeyboardList;
};

TEST(VirtualKeyboardManagerHandler, TestValidateUrl) {
  static const char* layouts1[] = { "a", "b" };
  static const char* layouts2[] = { "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts1), true);
  input_method::VirtualKeyboard virtual_keyboard_2(
      GURL("http://url2/"), "name 2", CreateLayoutSet(layouts2), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_2.url(),
      virtual_keyboard_2.name(),
      virtual_keyboard_2.supported_layouts(),
      virtual_keyboard_2.is_system()));

  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(2U, url_to_keyboard.size());

  EXPECT_TRUE(Testee::ValidateUrl(url_to_keyboard, "a", "http://url1/"));
  EXPECT_TRUE(Testee::ValidateUrl(url_to_keyboard, "b", "http://url1/"));
  EXPECT_TRUE(Testee::ValidateUrl(url_to_keyboard, "b", "http://url2/"));

  EXPECT_FALSE(Testee::ValidateUrl(url_to_keyboard, "a", "http://url3/"));
  EXPECT_FALSE(Testee::ValidateUrl(url_to_keyboard, "b", "http://url3/"));
  EXPECT_FALSE(Testee::ValidateUrl(url_to_keyboard, "c", "http://url1/"));
  EXPECT_FALSE(Testee::ValidateUrl(url_to_keyboard, "c", "http://url2/"));
}

TEST(VirtualKeyboardManagerHandler, TestSingleKeyboard) {
  static const char* layouts[] = { "a", "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(arraysize(layouts), layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(1U, url_to_keyboard.size());

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, NULL));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(arraysize(layouts), keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);
}

TEST(VirtualKeyboardManagerHandler, TestSingleKeyboardWithPref) {
  static const char* layouts[] = { "a", "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(arraysize(layouts), layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(1U, url_to_keyboard.size());

  // create pref object.
  scoped_ptr<DictionaryValue> pref(new DictionaryValue);
  pref->SetString("b", "http://url1/");

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, pref.get()));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(arraysize(layouts), keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_TRUE(dictionary_value->GetString("preferredKeyboard", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);
}

TEST(VirtualKeyboardManagerHandler, TestSingleKeyboardWithTwoPrefs) {
  static const char* layouts[] = { "a", "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(arraysize(layouts), layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(1U, url_to_keyboard.size());

  // create pref object.
  scoped_ptr<DictionaryValue> pref(new DictionaryValue);
  pref->SetString("a", "http://url1/");
  pref->SetString("b", "http://url1/");

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, pref.get()));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(arraysize(layouts), keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_TRUE(dictionary_value->GetString("preferredKeyboard", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_TRUE(dictionary_value->GetString("preferredKeyboard", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);
}

TEST(VirtualKeyboardManagerHandler, TestSingleKeyboardWithBadPref1) {
  static const char* layouts[] = { "a", "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(arraysize(layouts), layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(1U, url_to_keyboard.size());

  // create pref object.
  scoped_ptr<DictionaryValue> pref(new DictionaryValue);
  pref->SetString("unknownlayout", "http://url1/");

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, pref.get()));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(arraysize(layouts), keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);
}

TEST(VirtualKeyboardManagerHandler, TestSingleKeyboardWithBadPref2) {
  static const char* layouts[] = { "a", "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(arraysize(layouts), layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(1U, url_to_keyboard.size());

  // create pref object.
  scoped_ptr<DictionaryValue> pref(new DictionaryValue);
  pref->SetString("a", "http://unknownurl/");

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, pref.get()));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(arraysize(layouts), keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);
}

TEST(VirtualKeyboardManagerHandler, TestSingleKeyboardWithBadPref3) {
  static const char* layout1[] = { "a" };
  static const char* layout2[] = { "b" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layout1), true);
  input_method::VirtualKeyboard virtual_keyboard_2(
      GURL("http://url2/"), "name 2", CreateLayoutSet(layout2), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_2.url(),
      virtual_keyboard_2.name(),
      virtual_keyboard_2.supported_layouts(),
      virtual_keyboard_2.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(2U, layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(2U, url_to_keyboard.size());

  // create pref object.
  scoped_ptr<DictionaryValue> pref(new DictionaryValue);
  pref->SetString("a", "http://url2/");  // url2 does not support "a".

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, pref.get()));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(2U, keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url2/", string_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 2", string_value);
}

TEST(VirtualKeyboardManagerHandler, TestMultipleKeyboards) {
  static const char* layouts1[] = { "a", "b" };
  static const char* layouts2[] = { "c" };
  static const char* layouts3[] = { "b", "d" };
  input_method::VirtualKeyboard virtual_keyboard_1(
      GURL("http://url1/"), "name 1", CreateLayoutSet(layouts1), true);
  input_method::VirtualKeyboard virtual_keyboard_2(
      GURL("http://url2/"), "name 2", CreateLayoutSet(layouts2), false);
  input_method::VirtualKeyboard virtual_keyboard_3(
      GURL("http://url3/"), "name 3", CreateLayoutSet(layouts3), true);

  input_method::VirtualKeyboardSelector selector;
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_1.url(),
      virtual_keyboard_1.name(),
      virtual_keyboard_1.supported_layouts(),
      virtual_keyboard_1.is_system()));
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_2.url(),
      virtual_keyboard_2.name(),
      virtual_keyboard_2.supported_layouts(),
      virtual_keyboard_2.is_system()));
  ASSERT_TRUE(selector.AddVirtualKeyboard(
      virtual_keyboard_3.url(),
      virtual_keyboard_3.name(),
      virtual_keyboard_3.supported_layouts(),
      virtual_keyboard_3.is_system()));

  const LayoutToKeyboard& layout_to_keyboard = selector.layout_to_keyboard();
  ASSERT_EQ(arraysize(layouts1) + arraysize(layouts2) + arraysize(layouts3),
            layout_to_keyboard.size());
  const UrlToKeyboard& url_to_keyboard = selector.url_to_keyboard();
  ASSERT_EQ(3U, url_to_keyboard.size());

  scoped_ptr<ListValue> keyboards(Testee::CreateVirtualKeyboardList(
      layout_to_keyboard, url_to_keyboard, NULL));
  ASSERT_TRUE(keyboards.get());
  ASSERT_EQ(4U /* a, b, c, and d */, keyboards->GetSize());

  DictionaryValue* dictionary_value;
  std::string string_value;
  ListValue* list_value;
  bool boolean_value = false;

  // Check the first element (for the layout "a").
  ASSERT_TRUE(keyboards->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("a", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetBoolean("isSystem", &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);

  // Check the second element (for the layout "b").
  ASSERT_TRUE(keyboards->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("b", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(2U, list_value->GetSize());  // keyboard1 and 3.
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url1/", string_value);
  EXPECT_TRUE(dictionary_value->GetBoolean("isSystem", &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 1", string_value);
  ASSERT_TRUE(list_value->GetDictionary(1, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url3/", string_value);
  EXPECT_TRUE(dictionary_value->GetBoolean("isSystem", &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 3", string_value);

  // 3rd.
  ASSERT_TRUE(keyboards->GetDictionary(2, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("c", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url2/", string_value);
  EXPECT_TRUE(dictionary_value->GetBoolean("isSystem", &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 2", string_value);

  // 4th.
  ASSERT_TRUE(keyboards->GetDictionary(3, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("layout", &string_value));
  EXPECT_EQ("d", string_value);
  EXPECT_TRUE(dictionary_value->GetString("layoutName", &string_value));
  EXPECT_FALSE(dictionary_value->GetString("preferredKeyboard", &string_value));
  ASSERT_TRUE(dictionary_value->GetList("supportedKeyboards", &list_value));
  ASSERT_EQ(1U, list_value->GetSize());
  ASSERT_TRUE(list_value->GetDictionary(0, &dictionary_value));
  EXPECT_TRUE(dictionary_value->GetString("url", &string_value));
  EXPECT_EQ("http://url3/", string_value);
  EXPECT_TRUE(dictionary_value->GetBoolean("isSystem", &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(dictionary_value->GetString("name", &string_value));
  EXPECT_EQ("name 3", string_value);
}

}  // namespace chromeos
