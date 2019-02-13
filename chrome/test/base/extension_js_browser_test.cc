// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/extension_js_browser_test.h"

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"

ExtensionJSBrowserTest::ExtensionJSBrowserTest() : libs_loaded_(false) {
}

ExtensionJSBrowserTest::~ExtensionJSBrowserTest() {
}

void ExtensionJSBrowserTest::WaitForExtension(const char* extension_id,
                                              const base::Closure& load_cb) {
  load_waiter_.reset(new ExtensionLoadWaiterOneShot());
  load_waiter_->WaitForExtension(extension_id, load_cb);
}

bool ExtensionJSBrowserTest::RunJavascriptTestF(bool is_async,
                                                const std::string& test_fixture,
                                                const std::string& test_name) {
  EXPECT_TRUE(load_waiter_->browser_context());
  if (!load_waiter_->browser_context())
    return false;
  std::vector<base::Value> args;
  args.push_back(base::Value(test_fixture));
  args.push_back(base::Value(test_name));
  std::vector<base::string16> scripts;
  if (!libs_loaded_) {
    BuildJavascriptLibraries(&scripts);
    libs_loaded_ = true;
  }
  scripts.push_back(
      BuildRunTestJSCall(is_async, "RUN_TEST_F", std::move(args)));

  base::string16 script_16 =
      base::JoinString(scripts, base::ASCIIToUTF16("\n"));
  std::string script = base::UTF16ToUTF8(script_16);

  std::string result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          Profile::FromBrowserContext(load_waiter_->browser_context()),
          load_waiter_->extension_id(),
          script);

  std::unique_ptr<base::Value> value_result =
      base::JSONReader::ReadDeprecated(result);
  CHECK_EQ(base::Value::Type::DICTIONARY, value_result->type());
  base::DictionaryValue* dict_value =
      static_cast<base::DictionaryValue*>(value_result.get());
  bool test_result;
  std::string test_result_message;
  CHECK(dict_value->GetBoolean("result", &test_result));
  CHECK(dict_value->GetString("message", &test_result_message));
  if (!test_result_message.empty())
    ADD_FAILURE() << test_result_message;
  return test_result;
}
