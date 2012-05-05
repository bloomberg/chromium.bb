// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_test_utils.h"

#include <string>

#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace {

class TestFunctionDispatcherDelegate
    : public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestFunctionDispatcherDelegate(Browser* browser) :
      browser_(browser) {}
  virtual ~TestFunctionDispatcherDelegate() {}

 private:
  virtual ExtensionWindowController* GetExtensionWindowController()
      const OVERRIDE {
    return browser_->extension_window_controller();
  }

  virtual WebContents* GetAssociatedWebContents() const OVERRIDE {
    return NULL;
  }

  Browser* browser_;
};

}  // namespace

namespace extension_function_test_utils {

base::Value* ParseJSON(const std::string& data) {
  return base::JSONReader::Read(data);
}

base::ListValue* ParseList(const std::string& data) {
  scoped_ptr<base::Value> result(ParseJSON(data));
  if (result.get() && result->IsType(base::Value::TYPE_LIST))
    return static_cast<base::ListValue*>(result.release());
  else
    return NULL;
}

base::DictionaryValue* ParseDictionary(
    const std::string& data) {
  scoped_ptr<base::Value> result(ParseJSON(data));
  if (result.get() && result->IsType(base::Value::TYPE_DICTIONARY))
    return static_cast<base::DictionaryValue*>(result.release());
  else
    return NULL;
}

bool GetBoolean(base::DictionaryValue* val, const std::string& key) {
  bool result = false;
  if (!val->GetBoolean(key, &result))
      ADD_FAILURE() << key << " does not exist or is not a boolean.";
  return result;
}

int GetInteger(base::DictionaryValue* val, const std::string& key) {
  int result = 0;
  if (!val->GetInteger(key, &result))
    ADD_FAILURE() << key << " does not exist or is not an integer.";
  return result;
}

std::string GetString(base::DictionaryValue* val, const std::string& key) {
  std::string result;
  if (!val->GetString(key, &result))
    ADD_FAILURE() << key << " does not exist or is not a string.";
  return result;
}

base::DictionaryValue* ToDictionary(base::Value* val) {
  EXPECT_TRUE(val);
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, val->GetType());
  return static_cast<base::DictionaryValue*>(val);
}

base::ListValue* ToList(base::Value* val) {
  EXPECT_TRUE(val);
  EXPECT_EQ(base::Value::TYPE_LIST, val->GetType());
  return static_cast<base::ListValue*>(val);
}

scoped_refptr<Extension> CreateEmptyExtension() {
  return CreateEmptyExtensionWithLocation(Extension::INTERNAL);
}

scoped_refptr<Extension> CreateEmptyExtensionWithLocation(
    Extension::Location location) {
  std::string error;
  const FilePath test_extension_path;
  scoped_ptr<base::DictionaryValue> test_extension_value(
      ParseDictionary("{\"name\": \"Test\", \"version\": \"1.0\"}"));
  scoped_refptr<Extension> extension(Extension::Create(
      test_extension_path,
      location,
      *test_extension_value.get(),
      Extension::NO_FLAGS,
      &error));
  EXPECT_TRUE(error.empty()) << "Could not parse test extension " << error;
  return extension;
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
  EXPECT_FALSE(function->GetResultValue()) << "Did not expect a result";
  return function->GetError();
}

base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                        const std::string& args,
                                        Browser* browser) {
  return RunFunctionAndReturnResult(function, args, browser, NONE);
}
base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                        const std::string& args,
                                        Browser* browser,
                                        RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
      << function->GetError();
  return (function->GetResultValue() == NULL) ? NULL :
      function->GetResultValue()->DeepCopy();
}

// This helps us be able to wait until an AsyncExtensionFunction calls
// SendResponse.
class SendResponseDelegate
    : public UIThreadExtensionFunction::DelegateForTests {
 public:
  SendResponseDelegate() : should_post_quit_(false) {}

  virtual ~SendResponseDelegate() {}

  void set_should_post_quit(bool should_quit) {
    should_post_quit_ = should_quit;
  }

  bool HasResponse() {
    return response_.get() != NULL;
  }

  bool GetResponse() {
    EXPECT_TRUE(HasResponse());
    return *response_.get();
  }

  virtual void OnSendResponse(UIThreadExtensionFunction* function,
                              bool success,
                              bool bad_message) {
    ASSERT_FALSE(bad_message);
    ASSERT_FALSE(HasResponse());
    response_.reset(new bool);
    *response_ = success;
    if (should_post_quit_) {
      MessageLoopForUI::current()->Quit();
    }
  }

 private:
  scoped_ptr<bool> response_;
  bool should_post_quit_;
};

bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 RunFunctionFlags flags) {
  SendResponseDelegate response_delegate;
  function->set_test_delegate(&response_delegate);
  scoped_ptr<base::ListValue> parsed_args(ParseList(args));
  EXPECT_TRUE(parsed_args.get()) <<
      "Could not parse extension function arguments: " << args;
  function->SetArgs(parsed_args.get());

  TestFunctionDispatcherDelegate dispatcher_delegate(browser);
  ExtensionFunctionDispatcher dispatcher(
      browser->profile(), &dispatcher_delegate);
  function->set_dispatcher(dispatcher.AsWeakPtr());

  function->set_profile(browser->profile());
  function->set_include_incognito(flags & INCLUDE_INCOGNITO);
  function->Run();

  // If the RunImpl of |function| didn't already call SendResponse, run the
  // message loop until they do.
  if (!response_delegate.HasResponse()) {
    response_delegate.set_should_post_quit(true);
    ui_test_utils::RunMessageLoop();
  }

  EXPECT_TRUE(response_delegate.HasResponse());
  return response_delegate.GetResponse();
}

} // namespace extension_function_test_utils
