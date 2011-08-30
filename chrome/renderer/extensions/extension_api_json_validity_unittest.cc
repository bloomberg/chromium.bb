// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/extension_base.h"
#include "chrome/test/base/v8_unit_test.h"
#include "content/common/json_value_serializer.h"
#include "grit/common_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Given a ListValue* of dictionaries, find the first dictionary with
// a property |property| whose string value is |expected_value|.  The
// following conditions result in a return value of false, and an
// explanation in |out_error_message|:
// * Any list element is not a dictionary.
// * Any list element does not have a string property |property|.
// * More than one list element has a string property with value
//   |expected_value|.
//
// Return true and set |out_matching_dict| if such a dictionary was
// found.  If false is returned, |out_message| is set to an explanation.
bool FindDictionaryWithProperyValue(ListValue* list,
                                    const std::string& property,
                                    const std::string& expected_value,
                                    DictionaryValue** out_matching_dict,
                                    std::string* out_error_message) {
  bool found = false;
  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    if (!(*it)->IsType(Value::TYPE_DICTIONARY)) {
      *out_error_message = "List contains an item taht is not a dictionary.";
      return false;
    }

    DictionaryValue* dict = static_cast<DictionaryValue*>(*it);

    std::string actual_value;
    if (!dict->GetStringASCII(property, &actual_value)) {
      *out_error_message =
          "Dictionary has no string property \'" + property + "'.";
      return false;
    }

    if (actual_value != expected_value)
      continue;

    if (found) {
      *out_error_message = "More than one dictionary matches.";
      return false;
    }
    found = true;
    if (out_matching_dict)
      *out_matching_dict = dict;
  }
  if (!found) {
    *out_error_message = "No dictionary matches.";
    return false;
  }
  return true;
}

}

class ExtensionApiJsonValidityTest : public V8UnitTest {
};

// Read extension_api.json with JSONFileValueSerializer.  This test checks
// that the file is valid without using V8 or grit generated code.
// Unlike V8's JSON.Parse(), it produces easy to read error messages.
// If this test passes, and ExtensionApiJsonValidityTest.WithV8 fails,
// check to see if V8 or the grit build step is broken.
TEST_F(ExtensionApiJsonValidityTest, Basic) {
  // Build the path to extension_api.json, and check that the file exists.
  FilePath extension_api_json;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &extension_api_json));
  extension_api_json = extension_api_json.AppendASCII("chrome")
      .AppendASCII("common")
      .AppendASCII("extensions")
      .AppendASCII("api")
      .AppendASCII("extension_api.json");
  ASSERT_TRUE(file_util::PathExists(extension_api_json))
      << "Failed to find file 'extension_api.json' at the following path: "
      << extension_api_json.value();

  std::string error_message;

  // Check that extension_api.json can be parsed as JSON.
  JSONFileValueSerializer serializer(extension_api_json);
  scoped_ptr<Value> root(serializer.Deserialize(NULL, &error_message));
  ASSERT_TRUE(root.get()) << error_message;
  ASSERT_EQ(Value::TYPE_LIST, root->GetType());
  ListValue* root_list = static_cast<ListValue*>(root.get());

  // As a basic smoke test that the JSON we read is reasonable, find the
  // definition of the functions chrome.test.log() and
  // chrome.test.notifyPass().
  DictionaryValue* test_namespace_dict;
  ASSERT_TRUE(FindDictionaryWithProperyValue(
      root_list, "namespace", "test", &test_namespace_dict, &error_message))
      << error_message;

  ListValue* functions_list;
  ASSERT_TRUE(test_namespace_dict->GetList("functions", &functions_list))
              << "Namespace 'test' should define some functions.";

  EXPECT_TRUE(FindDictionaryWithProperyValue(
      functions_list, "name", "log", NULL, &error_message))
      << error_message;

  EXPECT_TRUE(FindDictionaryWithProperyValue(
      functions_list, "name", "notifyPass", NULL, &error_message))
      << error_message;
}

// Crashes on Vista, see http://crbug.com/43855
#if defined(OS_WIN)
#define MAYBE_WithV8 DISABLED_WithV8
#else
#define MAYBE_WithV8 WithV8
#endif

// Use V8 to load the string resource version of extension_api.json .
// This test mimics the method extension_api.json is loaded in
// chrome/renderer/resources/extension_process_bindings.js .
TEST_F(ExtensionApiJsonValidityTest, MAYBE_WithV8) {
  std::string ext_api_string =
      ExtensionBase::GetStringResource(IDR_EXTENSION_API_JSON);

  // Create a global variable holding the text of extension_api.json .
  SetGlobalStringVar("ext_api_json_text", ext_api_string);

  // Parse the text of extension_api.json .  If there is a parse error,
  // an exception will be printed that includes a line number.
  std::string test_js = "var extension_api = JSON.parse(ext_api_json_text);";
  ExecuteScriptInContext(test_js, "ParseExtensionApiJson");
}
