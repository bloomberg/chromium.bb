// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api_test_utils.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionFunctionDispatcher;

namespace {

class TestFunctionDispatcherDelegate
    : public ExtensionFunctionDispatcher::Delegate {
 public:
  TestFunctionDispatcherDelegate() {}
  ~TestFunctionDispatcherDelegate() override {}

  // NULL implementation.
 private:
  DISALLOW_COPY_AND_ASSIGN(TestFunctionDispatcherDelegate);
};

base::Value* ParseJSON(const std::string& data) {
  return base::JSONReader::Read(data);
}

base::ListValue* ParseList(const std::string& data) {
  base::Value* result = ParseJSON(data);
  base::ListValue* list = NULL;
  result->GetAsList(&list);
  return list;
}

// This helps us be able to wait until an UIThreadExtensionFunction calls
// SendResponse.
class SendResponseDelegate
    : public UIThreadExtensionFunction::DelegateForTests {
 public:
  SendResponseDelegate() : should_post_quit_(false) {}

  virtual ~SendResponseDelegate() {}

  void set_should_post_quit(bool should_quit) {
    should_post_quit_ = should_quit;
  }

  bool HasResponse() { return response_.get() != NULL; }

  bool GetResponse() {
    EXPECT_TRUE(HasResponse());
    return *response_.get();
  }

  void OnSendResponse(UIThreadExtensionFunction* function,
                      bool success,
                      bool bad_message) override {
    ASSERT_FALSE(bad_message);
    ASSERT_FALSE(HasResponse());
    response_.reset(new bool);
    *response_ = success;
    if (should_post_quit_) {
      base::MessageLoopForUI::current()->Quit();
    }
  }

 private:
  scoped_ptr<bool> response_;
  bool should_post_quit_;
};

}  // namespace

namespace extensions {

namespace api_test_utils {

base::DictionaryValue* ParseDictionary(const std::string& data) {
  base::Value* result = ParseJSON(data);
  base::DictionaryValue* dict = NULL;
  result->GetAsDictionary(&dict);
  return dict;
}

bool GetBoolean(const base::DictionaryValue* val, const std::string& key) {
  bool result = false;
  if (!val->GetBoolean(key, &result))
    ADD_FAILURE() << key << " does not exist or is not a boolean.";
  return result;
}

int GetInteger(const base::DictionaryValue* val, const std::string& key) {
  int result = 0;
  if (!val->GetInteger(key, &result))
    ADD_FAILURE() << key << " does not exist or is not an integer.";
  return result;
}

std::string GetString(const base::DictionaryValue* val,
                      const std::string& key) {
  std::string result;
  if (!val->GetString(key, &result))
    ADD_FAILURE() << key << " does not exist or is not a string.";
  return result;
}

scoped_refptr<Extension> CreateExtension(
    Manifest::Location location,
    base::DictionaryValue* test_extension_value,
    const std::string& id_input) {
  std::string error;
  const base::FilePath test_extension_path;
  std::string id;
  if (!id_input.empty())
    id = crx_file::id_util::GenerateId(id_input);
  scoped_refptr<Extension> extension(
      Extension::Create(test_extension_path, location, *test_extension_value,
                        Extension::NO_FLAGS, id, &error));
  EXPECT_TRUE(error.empty()) << "Could not parse test extension " << error;
  return extension;
}

scoped_refptr<Extension> CreateExtension(
    base::DictionaryValue* test_extension_value) {
  return CreateExtension(Manifest::INTERNAL, test_extension_value,
                         std::string());
}

scoped_refptr<Extension> CreateEmptyExtensionWithLocation(
    Manifest::Location location) {
  scoped_ptr<base::DictionaryValue> test_extension_value(
      ParseDictionary("{\"name\": \"Test\", \"version\": \"1.0\"}"));
  return CreateExtension(location, test_extension_value.get(), std::string());
}

base::Value* RunFunctionWithDelegateAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    scoped_ptr<extensions::ExtensionFunctionDispatcher> dispatcher) {
  return RunFunctionWithDelegateAndReturnSingleResult(
      function, args, context, dispatcher.Pass(), NONE);
}

base::Value* RunFunctionWithDelegateAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    scoped_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  // Without a callback the function will not generate a result.
  function->set_has_callback(true);
  RunFunction(function, args, context, dispatcher.Pass(), flags);
  EXPECT_TRUE(function->GetError().empty())
      << "Unexpected error: " << function->GetError();
  const base::Value* single_result = NULL;
  if (function->GetResultList() != NULL &&
      function->GetResultList()->Get(0, &single_result)) {
    return single_result->DeepCopy();
  }
  return NULL;
}

base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context) {
  return RunFunctionAndReturnSingleResult(function, args, context, NONE);
}

base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    RunFunctionFlags flags) {
  TestFunctionDispatcherDelegate delegate;
  scoped_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(context, &delegate));

  return RunFunctionWithDelegateAndReturnSingleResult(
      function, args, context, dispatcher.Pass(), flags);
}

std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context) {
  return RunFunctionAndReturnError(function, args, context, NONE);
}

std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context,
                                      RunFunctionFlags flags) {
  TestFunctionDispatcherDelegate delegate;
  scoped_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(context, &delegate));
  scoped_refptr<ExtensionFunction> function_owner(function);
  // Without a callback the function will not generate a result.
  function->set_has_callback(true);
  RunFunction(function, args, context, dispatcher.Pass(), flags);
  EXPECT_FALSE(function->GetResultList()) << "Did not expect a result";
  return function->GetError();
}

bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 content::BrowserContext* context) {
  TestFunctionDispatcherDelegate delegate;
  scoped_ptr<ExtensionFunctionDispatcher> dispatcher(
      new ExtensionFunctionDispatcher(context, &delegate));
  return RunFunction(function, args, context, dispatcher.Pass(), NONE);
}

bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 content::BrowserContext* context,
                 scoped_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags) {
  scoped_ptr<base::ListValue> parsed_args(ParseList(args));
  EXPECT_TRUE(parsed_args.get())
      << "Could not parse extension function arguments: " << args;
  return RunFunction(
      function, parsed_args.Pass(), context, dispatcher.Pass(), flags);
}

bool RunFunction(UIThreadExtensionFunction* function,
                 scoped_ptr<base::ListValue> args,
                 content::BrowserContext* context,
                 scoped_ptr<extensions::ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags) {
  SendResponseDelegate response_delegate;
  function->set_test_delegate(&response_delegate);
  function->SetArgs(args.get());

  CHECK(dispatcher);
  function->set_dispatcher(dispatcher->AsWeakPtr());

  function->set_browser_context(context);
  function->set_include_incognito(flags & INCLUDE_INCOGNITO);
  function->Run()->Execute();

  // If the RunAsync of |function| didn't already call SendResponse, run the
  // message loop until they do.
  if (!response_delegate.HasResponse()) {
    response_delegate.set_should_post_quit(true);
    content::RunMessageLoop();
  }

  EXPECT_TRUE(response_delegate.HasResponse());
  return response_delegate.GetResponse();
}

}  // namespace api_test_utils
}  // namespace extensions
