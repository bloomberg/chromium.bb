// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_test_utils.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using extensions::Extension;
using extensions::Manifest;
namespace keys = extensions::tabs_constants;

namespace {

class TestFunctionDispatcherDelegate
    : public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestFunctionDispatcherDelegate(Browser* browser) :
      browser_(browser) {}
  ~TestFunctionDispatcherDelegate() override {}

 private:
  extensions::WindowController* GetExtensionWindowController() const override {
    return browser_->extension_window_controller();
  }

  WebContents* GetAssociatedWebContents() const override { return NULL; }

  Browser* browser_;
};

}  // namespace

namespace extension_function_test_utils {

base::ListValue* ParseList(const std::string& data) {
  std::unique_ptr<base::Value> result = base::JSONReader::Read(data);
  base::ListValue* list = NULL;
  result->GetAsList(&list);
  ignore_result(result.release());
  return list;
}

base::DictionaryValue* ToDictionary(base::Value* val) {
  EXPECT_TRUE(val);
  EXPECT_EQ(base::Value::Type::DICTIONARY, val->type());
  return static_cast<base::DictionaryValue*>(val);
}

base::ListValue* ToList(base::Value* val) {
  EXPECT_TRUE(val);
  EXPECT_EQ(base::Value::Type::LIST, val->type());
  return static_cast<base::ListValue*>(val);
}

bool HasPrivacySensitiveFields(base::DictionaryValue* val) {
  std::string result;
  if (val->GetString(keys::kUrlKey, &result) ||
      val->GetString(keys::kTitleKey, &result) ||
      val->GetString(keys::kFaviconUrlKey, &result))
    return true;
  return false;
}

std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser) {
  return RunFunctionAndReturnError(function, args, browser, NONE);
}
std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser,
                                      RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  // When sending a response, the function will set an empty list value if there
  // is no specified result.
  const base::ListValue* results = function->GetResultList();
  CHECK(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
  return function->GetError();
}

base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    Browser* browser) {
  return RunFunctionAndReturnSingleResult(function, args, browser, NONE);
}
base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    Browser* browser,
    RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
      << function->GetError();
  const base::Value* single_result = NULL;
  if (function->GetResultList() != NULL &&
      function->GetResultList()->Get(0, &single_result)) {
    return single_result->DeepCopy();
  }
  return NULL;
}

bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 RunFunctionFlags flags) {
  std::unique_ptr<base::ListValue> parsed_args(ParseList(args));
  EXPECT_TRUE(parsed_args.get())
      << "Could not parse extension function arguments: " << args;
  return RunFunction(function, std::move(parsed_args), browser, flags);
}

bool RunFunction(UIThreadExtensionFunction* function,
                 std::unique_ptr<base::ListValue> args,
                 Browser* browser,
                 RunFunctionFlags flags) {
  TestFunctionDispatcherDelegate dispatcher_delegate(browser);
  std::unique_ptr<extensions::ExtensionFunctionDispatcher> dispatcher(
      new extensions::ExtensionFunctionDispatcher(browser->profile()));
  dispatcher->set_delegate(&dispatcher_delegate);
  // TODO(yoz): The cast is a hack; these flags should be defined in
  // only one place.  See crbug.com/394840.
  return extensions::api_test_utils::RunFunction(
      function, std::move(args), browser->profile(), std::move(dispatcher),
      static_cast<extensions::api_test_utils::RunFunctionFlags>(flags));
}

} // namespace extension_function_test_utils
