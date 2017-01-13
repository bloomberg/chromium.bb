// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_request_handler.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

const char kEchoArgs[] =
    "(function() { this.result = Array.from(arguments); })";

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
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)));

  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id =
      request_handler.AddPendingRequest(isolate(), function, context);
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  const char kArguments[] = "['foo',1,{'prop1':'bar'}]";
  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString(kArguments);
  ASSERT_TRUE(response_arguments);
  request_handler.CompleteRequest(request_id, *response_arguments);

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
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)));

  v8::Local<v8::Function> function = FunctionFromString(context, kEchoArgs);
  ASSERT_FALSE(function.IsEmpty());

  int request_id =
      request_handler.AddPendingRequest(isolate(), function, context);
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_id));

  std::unique_ptr<base::ListValue> response_arguments =
      ListValueFromString("['foo']");
  ASSERT_TRUE(response_arguments);

  // Try running with a non-existent request id.
  int fake_request_id = 42;
  request_handler.CompleteRequest(fake_request_id, *response_arguments);
  EXPECT_FALSE(did_run_js());

  // Try running with a request from an invalidated context.
  request_handler.InvalidateContext(context);
  request_handler.CompleteRequest(request_id, *response_arguments);
  EXPECT_FALSE(did_run_js());
}

TEST_F(APIRequestHandlerTest, MultipleRequestsAndContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = ContextLocal();
  v8::Local<v8::Context> context_b = v8::Context::New(isolate());
  gin::ContextHolder holder_b(isolate());
  holder_b.SetContext(context_b);

  APIRequestHandler request_handler(
      base::Bind(&APIRequestHandlerTest::RunJS, base::Unretained(this)));

  // By having both different arguments and different behaviors in the
  // callbacks, we can easily verify that the right function is called in the
  // right context.
  v8::Local<v8::Function> function_a = FunctionFromString(
      context_a, "(function(res) { this.result = res + 'alpha'; })");
  v8::Local<v8::Function> function_b = FunctionFromString(
      context_b, "(function(res) { this.result = res + 'beta'; })");

  int request_a =
      request_handler.AddPendingRequest(isolate(), function_a, context_a);
  int request_b =
      request_handler.AddPendingRequest(isolate(), function_b, context_b);

  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_a, request_b));

  std::unique_ptr<base::ListValue> response_a =
      ListValueFromString("['response_a:']");
  ASSERT_TRUE(response_a);

  request_handler.CompleteRequest(request_a, *response_a);
  EXPECT_TRUE(did_run_js());
  EXPECT_THAT(request_handler.GetPendingRequestIdsForTesting(),
              testing::UnorderedElementsAre(request_b));

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_a:alpha'"),
      GetStringPropertyFromObject(context_a->Global(), context_a, "result"));

  std::unique_ptr<base::ListValue> response_b =
      ListValueFromString("['response_b:']");
  ASSERT_TRUE(response_b);

  request_handler.CompleteRequest(request_b, *response_b);
  EXPECT_TRUE(request_handler.GetPendingRequestIdsForTesting().empty());

  EXPECT_EQ(
      ReplaceSingleQuotes("'response_b:beta'"),
      GetStringPropertyFromObject(context_b->Global(), context_b, "result"));
}

}  // namespace extensions
