// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_request_handler.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
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
  v8::Local<v8::Context> context = ContextLocal();

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent()));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler.StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(), function,
      v8::Local<v8::Function>());
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString(kArguments);
  ASSERT_TRUE(response_arguments);
  request_handler.CompleteRequest(request_id, *response_arguments,
                                  std::string());

  EXPECT_TRUE(did_run_js());
  EXPECT_EQ(ReplaceSingleQuotes(kArguments),
            GetStringPropertyFromObject(context->Global(), context, "result"));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
}

// Tests that trying to run non-existent or invalided requests is a no-op.
TEST_F(APIRequestHandlerTest, InvalidRequestsTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent()));

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id = request_handler.StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(), function,
      v8::Local<v8::Function>());
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['foo']");
  ASSERT_TRUE(response_arguments);

  // Try running with a non-existent request id.
  int fake_request_id = 42;
  request_handler.CompleteRequest(fake_request_id, *response_arguments,
                                  std::string());
  EXPECT_FALSE(did_run_js());

  // Try running with a request from an invalidated context.
  request_handler.InvalidateContext(context);
  request_handler.CompleteRequest(request_id, *response_arguments,
                                  std::string());
  EXPECT_FALSE(did_run_js());
}

TEST_F(APIRequestHandlerTest, MultipleRequestsAndContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = ContextLocal();
  v8::Local<v8::Context> context_b = v8::Context::New(isolate());
  gin::ContextHolder holder_b(isolate());
  holder_b.SetContext(context_b);

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent()));

  // By having both different arguments and different behaviors in the
  // callbacks, we can easily verify that the right function is called in the
  // right context.
  v8::Local<v8::Function> function_a = FunctionFromString(
      context_a, "(function(res) { this.result = res + 'alpha'; })");
  v8::Local<v8::Function> function_b = FunctionFromString(
      context_b, "(function(res) { this.result = res + 'beta'; })");

  int request_a = request_handler.StartRequest(
      context_a, kMethod, base::MakeUnique<base::ListValue>(), function_a,
      v8::Local<v8::Function>());
  int request_b = request_handler.StartRequest(
      context_b, kMethod, base::MakeUnique<base::ListValue>(), function_b,
      v8::Local<v8::Function>());

  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_a, request_b));

  std::unique_ptr<base::ListValue> response_a =
      ListValueFromString("['response_a:']");
  ASSERT_TRUE(response_a);

  request_handler.CompleteRequest(request_a, *response_a, std::string());
  EXPECT_TRUE(did_run_js());
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_b));

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_a:alpha'"),
      GetStringPropertyFromObject(context_a->Global(), context_a, "result"));

  std::unique_ptr<base::ListValue> response_b =
      ListValueFromString("['response_b:']");
  ASSERT_TRUE(response_b);

  request_handler.CompleteRequest(request_b, *response_b, std::string());
  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_b:beta'"),
      GetStringPropertyFromObject(context_b->Global(), context_b, "result"));
}

TEST_F(APIRequestHandlerTest, CustomCallbackArguments) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent()));

  v8::Local<v8::Function> custom_callback =
      FunctionFromString(context, kEchoArgs);
  v8::Local<v8::Function> callback =
      FunctionFromString(context, "(function() {})");
  ASSERT_FALSE(callback.IsEmpty());
  ASSERT_FALSE(custom_callback.IsEmpty());

  int request_id = request_handler.StartRequest(
      context, "method", base::MakeUnique<base::ListValue>(), callback,
      custom_callback);
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['response', 'arguments']");
  ASSERT_TRUE(response_arguments);
  request_handler.CompleteRequest(request_id, *response_arguments,
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
  EXPECT_EQ("{}", V8ToString(args[1], context));
  EXPECT_EQ(callback, args[2]);
  EXPECT_EQ("\"response\"", V8ToString(args[3], context));
  EXPECT_EQ("\"arguments\"", V8ToString(args[4], context));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());
}

// Test user gestures being curried around for API requests.
TEST_F(APIRequestHandlerTest, UserGestureTest) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  APIRequestHandler request_handler(
      base::Bind(&DoNothingWithRequest),
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)),
      APILastError(APILastError::GetParent()));

  auto callback = [](base::Optional<bool>* ran_with_user_gesture) {
    *ran_with_user_gesture =
        blink::WebUserGestureIndicator::isProcessingUserGestureThreadSafe();
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
  int request_id = request_handler.StartRequest(
      context, kMethod, base::MakeUnique<base::ListValue>(), v8_callback,
      v8::Local<v8::Function>());
  request_handler.CompleteRequest(request_id, *ListValueFromString("[]"),
                                  std::string());

  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_FALSE(*ran_with_user_gesture);
  ran_with_user_gesture.reset();

  // Next try calling with a user gesture. Since a gesture will be active at the
  // time of the call, it should also be active during the callback.
  {
    blink::WebScopedUserGesture user_gesture(nullptr);
    EXPECT_TRUE(
        blink::WebUserGestureIndicator::isProcessingUserGestureThreadSafe());
    request_id = request_handler.StartRequest(
        context, kMethod, base::MakeUnique<base::ListValue>(), v8_callback,
        v8::Local<v8::Function>());
  }
  EXPECT_FALSE(
      blink::WebUserGestureIndicator::isProcessingUserGestureThreadSafe());

  request_handler.CompleteRequest(request_id, *ListValueFromString("[]"),
                                  std::string());
  ASSERT_TRUE(ran_with_user_gesture);
  EXPECT_TRUE(*ran_with_user_gesture);
  // Sanity check - after the callback ran, there shouldn't be an active
  // gesture.
  EXPECT_FALSE(
      blink::WebUserGestureIndicator::isProcessingUserGestureThreadSafe());
}

}  // namespace extensions
