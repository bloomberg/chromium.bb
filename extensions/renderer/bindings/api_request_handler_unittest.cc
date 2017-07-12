// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_request_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/bindings/exception_handler.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace extensions {

namespace {

const char kEchoArgs[] =
    "(function() { this.result = Array.from(arguments); })";

const char kMethod[] = "method";

// TODO(devlin): We should probably hoist this up to e.g. api_binding_types.h.
using ArgumentList = std::vector<v8::Local<v8::Value>>;

// TODO(devlin): Should we move some parts of api_binding_unittest.cc to here?
void DoNothingWithRequest(std::unique_ptr<APIRequestHandler::Request> request,
                          v8::Local<v8::Context> context) {}

}  // namespace

class APIRequestHandlerTest : public APIBindingTest {
 public:
  // Runs the given |function|.
  void RunJS(v8::Local<v8::Function> function,
             v8::Local<v8::Context> context,
             int argc,
             v8::Local<v8::Value> argv[]) {
    RunFunctionOnGlobal(function, context, argc, argv);
    did_run_js_ = true;
  }

  std::unique_ptr<APIRequestHandler> CreateRequestHandler() {
    return base::MakeUnique<APIRequestHandler>(
        base::Bind(&DoNothingWithRequest),
        base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
        APILastError(APILastError::GetParent(), binding::AddConsoleError()),
        nullptr);
  }

 protected:
  APIRequestHandlerTest() {}
  ~APIRequestHandlerTest() override {}

  bool did_run_js() const { return did_run_js_; }

 private:
  bool did_run_js_ = false;

  DISALLOW_COPY_AND_ASSIGN(APIRequestHandlerTest);
};

// Tests adding a request to the request handler, and then triggering the
// response.
TEST_F(APIRequestHandlerTest, AddRequestAndCompleteRequestTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler->StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(), function,
      v8::Local<v8::Function>(), binding::RequestThread::UI);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString(kArguments);
  ASSERT_TRUE(response_arguments);
  request_handler->CompleteRequest(request_id, *response_arguments,
                                   std::string());

  EXPECT_TRUE(did_run_js());
  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  request_id = request_handler->StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(),
      v8::Local<v8::Function>(), v8::Local<v8::Function>(),
      binding::RequestThread::UI);
  EXPECT_NE(-1, request_id);
  request_handler->CompleteRequest(request_id, base::ListValue(),
                                   std::string());
}

// Tests that trying to run non-existent or invalided requests is a no-op.
TEST_F(APIRequestHandlerTest, InvalidRequestsTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler->StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(), function,
      v8::Local<v8::Function>(), binding::RequestThread::UI);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['foo']");
  ASSERT_TRUE(response_arguments);

  // Try running with a non-existent request id.
  int fake_request_id = 42;
  request_handler->CompleteRequest(fake_request_id, *response_arguments,
                                   std::string());
  EXPECT_FALSE(did_run_js());

  // Try running with a request from an invalidated context.
  request_handler->InvalidateContext(context);
  request_handler->CompleteRequest(request_id, *response_arguments,
                                   std::string());
  EXPECT_FALSE(did_run_js());
}

TEST_F(APIRequestHandlerTest, MultipleRequestsAndContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  // By having both different arguments and different behaviors in the
  // callbacks, we can easily verify that the right function is called in the
  // right context.
  v8::Local<v8::Function> function_a = FunctionFromString(
      context_a, "(function(res) { this.result = res + 'alpha'; })");
  v8::Local<v8::Function> function_b = FunctionFromString(
      context_b, "(function(res) { this.result = res + 'beta'; })");

  int request_a = request_handler->StartRequest(
      context_a, kMethod, base::MakeUnique<base::ListValue>(), function_a,
      v8::Local<v8::Function>(), binding::RequestThread::UI);
  int request_b = request_handler->StartRequest(
      context_b, kMethod, base::MakeUnique<base::ListValue>(), function_b,
      v8::Local<v8::Function>(), binding::RequestThread::UI);

  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_a, request_b));

  std::unique_ptr<base::ListValue> response_a =
      ListValueFromString("['response_a:']");
  ASSERT_TRUE(response_a);

  request_handler->CompleteRequest(request_a, *response_a, std::string());
  EXPECT_TRUE(did_run_js());
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_b));

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_a:alpha'"),
      GetStringPropertyFromObject(context_a->Global(), context_a, "result"));

  std::unique_ptr<base::ListValue> response_b =
      ListValueFromString("['response_b:']");
  ASSERT_TRUE(response_b);

  request_handler->CompleteRequest(request_b, *response_b, std::string());
  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_b:beta'"),
      GetStringPropertyFromObject(context_b->Global(), context_b, "result"));
}

TEST_F(APIRequestHandlerTest, CustomCallbackArguments) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kEchoArgs);
  v8::Local<v8::Function> callback =
      FunctionFromString(context, "(function() {})");
  ASSERT_FALSE(callback.IsEmpty());
  ASSERT_FALSE(custom_callback.IsEmpty());

  int request_id = request_handler->StartRequest(
      context, "method", base::MakeUnique<base::ListValue>(), callback,
      custom_callback, binding::RequestThread::UI);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['response', 'arguments']");
  ASSERT_TRUE(response_arguments);
  request_handler->CompleteRequest(request_id, *response_arguments,
                                   std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Value> result =
      GetPropertyFromObject(context->Global(), context, "result");
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(result->IsArray());
  ArgumentList args;
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(5u, args.size());
  EXPECT_EQ("\"method\"", V8ToString(args[0], context));
  EXPECT_EQ(base::StringPrintf("{\"id\":%d}", request_id),
            V8ToString(args[1], context));
  EXPECT_EQ(callback, args[2]);
  EXPECT_EQ("\"response\"", V8ToString(args[3], context));
  EXPECT_EQ("\"arguments\"", V8ToString(args[4], context));

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Test that having a custom callback without an extension-provided callback
// doesn't crash.
TEST_F(APIRequestHandlerTest, CustomCallbackArgumentsWithEmptyCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(custom_callback.IsEmpty());

  v8::Local<v8::Function> empty_callback;
  int request_id = request_handler->StartRequest(
      context, "method", base::MakeUnique<base::ListValue>(), empty_callback,
      custom_callback, binding::RequestThread::UI);
  EXPECT_THAT(request_handler->GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  request_handler->CompleteRequest(request_id, base::ListValue(),
                                   std::string());

  EXPECT_TRUE(did_run_js());
  v8::Local<v8::Value> result =
      GetPropertyFromObject(context->Global(), context, "result");
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(result->IsArray());
  ArgumentList args;
  ASSERT_TRUE(gin::Converter<ArgumentList>::FromV8(isolate(), result, &args));
  ASSERT_EQ(3u, args.size());
  EXPECT_EQ("\"method\"", V8ToString(args[0], context));
  EXPECT_EQ(base::StringPrintf("{\"id\":%d}", request_id),
            V8ToString(args[1], context));
  EXPECT_TRUE(args[2]->IsUndefined());

  EXPECT_TRUE(request_handler->GetPendingRequestIdsForTesting().empty());
}

// Test user gestures being curried around for API requests.
TEST_F(APIRequestHandlerTest, UserGestureTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  std::unique_ptr<APIRequestHandler> request_handler = CreateRequestHandler();

  auto callback = [](base::Optional<bool>* ran_with_user_gesture) {
    *ran_with_user_gesture =
        blink::WebUserGestureIndicator::IsProcessingUserGestureThreadSafe();
  };

  // Set up a callback to be used with the request so we can check if a user
  // gesture was active.
  base::Optional<bool> ran_with_user_gesture;
  v8::Local<v8::FunctionTemplate> function_template =
      gin::CreateFunctionTemplate(isolate(),
                                  base::Bind(callback, &ran_with_user_gesture));
  v8::Local<v8::Function> v8_callback =
      function_template->GetFunction(context).ToLocalChecked();

  // Try first without a user gesture.
  int request_id = request_handler->StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(), v8_callback,
      v8::Local<v8::Function>(), binding::RequestThread::UI);
  request_handler->CompleteRequest(request_id, *ListValueFromString("[]"),
                                   std::string());

  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_FALSE(*ran_with_user_gesture);
  ran_with_user_gesture.reset();

  // Next try calling with a user gesture. Since a gesture will be active at the
  // time of the call, it should also be active during the callback.
  {
    blink::WebScopedUserGesture user_gesture(nullptr);
    EXPECT_TRUE(
        blink::WebUserGestureIndicator::IsProcessingUserGestureThreadSafe());
    request_id = request_handler->StartRequest(
        context, kMethod, base::MakeUnique<base::ListValue>(), v8_callback,
        v8::Local<v8::Function>(), binding::RequestThread::UI);
  }
  EXPECT_FALSE(
      blink::WebUserGestureIndicator::IsProcessingUserGestureThreadSafe());

  request_handler->CompleteRequest(request_id, *ListValueFromString("[]"),
                                   std::string());
  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_TRUE(*ran_with_user_gesture);
  // Sanity check - after the callback ran, there shouldn't be an active
  // gesture.
  EXPECT_FALSE(
      blink::WebUserGestureIndicator::IsProcessingUserGestureThreadSafe());
}

TEST_F(APIRequestHandlerTest, RequestThread) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::Optional<binding::RequestThread> thread;
  auto on_request = [](base::Optional<binding::RequestThread>* thread_out,
                       std::unique_ptr<APIRequestHandler::Request> request,
                       v8::Local<v8::Context> context) {
    *thread_out = request->thread;
  };

  APIRequestHandler request_handler(
      base::Bind(on_request, &thread),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      nullptr);

  request_handler.StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(),
      v8::Local<v8::Function>(), v8::Local<v8::Function>(),
      binding::RequestThread::UI);
  ASSERT_TRUE(thread);
  EXPECT_EQ(binding::RequestThread::UI, *thread);
  thread.reset();

  request_handler.StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(),
      v8::Local<v8::Function>(), v8::Local<v8::Function>(),
      binding::RequestThread::IO);
  ASSERT_TRUE(thread);
  EXPECT_EQ(binding::RequestThread::IO, *thread);
  thread.reset();
}

TEST_F(APIRequestHandlerTest, SettingLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  base::Optional<std::string> logged_error;
  auto get_parent = [](v8::Local<v8::Context> context,
                       v8::Local<v8::Object>* secondary_parent) {
    return context->Global();
  };

  auto log_error = [](base::Optional<std::string>* logged_error,
                      v8::Local<v8::Context> context,
                      const std::string& error) { *logged_error = error; };

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(base::Bind(get_parent),
                   base::Bind(log_error, &logged_error)),
      nullptr);

  const char kReportExposedLastError[] =
      "(function() {\n"
      "  if (this.lastError)\n"
      "    this.seenLastError = this.lastError.message;\n"
      "})";
  auto get_exposed_error = [context]() {
    return GetStringPropertyFromObject(context->Global(), context,
                                       "seenLastError");
  };

  {
    // Test a successful function call. No last error should be emitted to the
    // console or exposed to the callback.
    v8::Local<v8::Function> callback =
        FunctionFromString(context, kReportExposedLastError);
    int request_id = request_handler.StartRequest(
        context, kMethod, base::MakeUnique<base::ListValue>(), callback,
        v8::Local<v8::Function>(), binding::RequestThread::UI);
    request_handler.CompleteRequest(request_id, base::ListValue(),
                                    std::string());
    EXPECT_FALSE(logged_error);
    EXPECT_EQ("undefined", get_exposed_error());
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error. Since the callback checks the
    // last error, no error should be logged to the console (but it should be
    // exposed to the callback).
    v8::Local<v8::Function> callback =
        FunctionFromString(context, kReportExposedLastError);
    int request_id = request_handler.StartRequest(
        context, kMethod, base::MakeUnique<base::ListValue>(), callback,
        v8::Local<v8::Function>(), binding::RequestThread::UI);
    request_handler.CompleteRequest(request_id, base::ListValue(),
                                    "some error");
    EXPECT_FALSE(logged_error);
    EXPECT_EQ("\"some error\"", get_exposed_error());
    logged_error.reset();
  }

  {
    // Test a function call resulting in an error that goes unchecked by the
    // callback. The error should be logged.
    v8::Local<v8::Function> callback =
        FunctionFromString(context, "(function() {})");
    int request_id = request_handler.StartRequest(
        context, kMethod, base::MakeUnique<base::ListValue>(), callback,
        v8::Local<v8::Function>(), binding::RequestThread::UI);
    request_handler.CompleteRequest(request_id, base::ListValue(),
                                    "some error");
    ASSERT_TRUE(logged_error);
    EXPECT_EQ("Unchecked runtime.lastError: some error", *logged_error);
    logged_error.reset();
  }
}

TEST_F(APIRequestHandlerTest, AddPendingRequest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  bool dispatched_request = false;
  auto handle_request = [](bool* dispatched_request,
                           std::unique_ptr<APIRequestHandler::Request> request,
                           v8::Local<v8::Context> context) {
    *dispatched_request = true;
  };

  APIRequestHandler request_handler(
      base::Bind(handle_request, &dispatched_request),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      nullptr);

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler.AddPendingRequest(context, function);
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));
  // Even though we add a pending request, we shouldn't have dispatched anything
  // because AddPendingRequest() is intended for renderer-side implementations.
  EXPECT_FALSE(dispatched_request);

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString(kArguments);
  ASSERT_TRUE(response_arguments);
  request_handler.CompleteRequest(request_id, *response_arguments,
                                  std::string());

  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
  EXPECT_FALSE(dispatched_request);
}

// Tests that throwing an exception in a callback is properly handled.
TEST_F(APIRequestHandlerTest, ThrowExceptionInCallback) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  auto add_console_error = [](base::Optional<std::string>* error_out,
                              v8::Local<v8::Context> context,
                              const std::string& error) { *error_out = error; };

  // RunFunction* from the test util assert no errors; provide a version that
  // allows them.
  auto run_function_and_allow_errors =
      [](v8::Local<v8::Function> function, v8::Local<v8::Context> context,
         int argc, v8::Local<v8::Value> argv[]) {
        ignore_result(function->Call(context, context->Global(), argc, argv));
      };

  base::Optional<std::string> logged_error;
  ExceptionHandler exception_handler(
      base::Bind(add_console_error, &logged_error),
      base::Bind(&RunFunctionOnGlobalAndIgnoreResult));

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(run_function_and_allow_errors),
      APILastError(APILastError::GetParent(), binding::AddConsoleError()),
      &exception_handler);

  v8::TryCatch outer_try_catch(isolate());
  v8::Local<v8::Function> callback_throwing_error =
      FunctionFromString(context, "(function() { throw new Error('hello'); })");
  int request_id =
      request_handler.AddPendingRequest(context, callback_throwing_error);
  request_handler.CompleteRequest(request_id, base::ListValue(), std::string());
  // |outer_try_catch| should not have caught an error. This is important to not
  // disrupt our bindings code (or other running JS) when asynchronously
  // returning from an API call. Instead, the error should be caught and handled
  // by the exception handler.
  EXPECT_FALSE(outer_try_catch.HasCaught());
  ASSERT_TRUE(logged_error);
  EXPECT_EQ("Error handling response: Uncaught Error: hello", *logged_error);
}

}  // namespace extensions
