// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_binding_js_util.h"

#include "base/bind.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/api_bindings_system.h"
#include "extensions/renderer/api_bindings_system_unittest.h"
#include "gin/handle.h"

namespace extensions {

class APIBindingJSUtilUnittest : public APIBindingsSystemTest {
 protected:
  APIBindingJSUtilUnittest() {}
  ~APIBindingJSUtilUnittest() override {}

  gin::Handle<APIBindingJSUtil> CreateUtil() {
    return gin::CreateHandle(
        isolate(),
        new APIBindingJSUtil(bindings_system()->type_reference_map(),
                             bindings_system()->request_handler(),
                             bindings_system()->event_handler(),
                             base::Bind(&RunFunctionOnGlobalAndIgnoreResult)));
  }

  v8::Local<v8::Object> GetLastErrorParent(
      v8::Local<v8::Context> context) override {
    return context->Global();
  }

  std::string GetExposedError(v8::Local<v8::Context> context) {
    v8::Local<v8::Value> last_error =
        GetPropertyFromObject(context->Global(), context, "lastError");

    // Use ADD_FAILURE() to avoid messing up the return type with ASSERT.
    if (last_error.IsEmpty()) {
      ADD_FAILURE();
      return std::string();
    }
    if (!last_error->IsObject() && !last_error->IsUndefined()) {
      ADD_FAILURE();
      return std::string();
    }

    if (last_error->IsUndefined())
      return "[undefined]";
    return GetStringPropertyFromObject(last_error.As<v8::Object>(), context,
                                       "message");
  }

  APILastError* last_error() {
    return bindings_system()->request_handler()->last_error();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(APIBindingJSUtilUnittest);
};

TEST_F(APIBindingJSUtilUnittest, TestSetLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  gin::Handle<APIBindingJSUtil> util = CreateUtil();
  v8::Local<v8::Object> v8_util = util.ToV8().As<v8::Object>();

  EXPECT_FALSE(last_error()->HasError(context));
  EXPECT_EQ("[undefined]", GetExposedError(context));
  const char kSetLastError[] = "obj.setLastError('a last error');";
  CallFunctionOnObject(context, v8_util, kSetLastError);
  EXPECT_TRUE(last_error()->HasError(context));
  EXPECT_EQ("\"a last error\"", GetExposedError(context));

  CallFunctionOnObject(context, v8_util,
                       "obj.setLastError('a new last error')");
  EXPECT_TRUE(last_error()->HasError(context));
  EXPECT_EQ("\"a new last error\"", GetExposedError(context));
}

TEST_F(APIBindingJSUtilUnittest, TestHasLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  gin::Handle<APIBindingJSUtil> util = CreateUtil();
  v8::Local<v8::Object> v8_util = util.ToV8().As<v8::Object>();

  EXPECT_FALSE(last_error()->HasError(context));
  EXPECT_EQ("[undefined]", GetExposedError(context));
  const char kHasLastError[] = "return obj.hasLastError();";
  v8::Local<v8::Value> has_error =
      CallFunctionOnObject(context, v8_util, kHasLastError);
  EXPECT_EQ("false", V8ToString(has_error, context));

  last_error()->SetError(context, "an error");
  EXPECT_TRUE(last_error()->HasError(context));
  EXPECT_EQ("\"an error\"", GetExposedError(context));
  has_error = CallFunctionOnObject(context, v8_util, kHasLastError);
  EXPECT_EQ("true", V8ToString(has_error, context));

  last_error()->ClearError(context, false);
  EXPECT_FALSE(last_error()->HasError(context));
  EXPECT_EQ("[undefined]", GetExposedError(context));
  has_error = CallFunctionOnObject(context, v8_util, kHasLastError);
  EXPECT_EQ("false", V8ToString(has_error, context));
}

TEST_F(APIBindingJSUtilUnittest, TestRunWithLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  gin::Handle<APIBindingJSUtil> util = CreateUtil();
  v8::Local<v8::Object> v8_util = util.ToV8().As<v8::Object>();

  EXPECT_FALSE(last_error()->HasError(context));
  EXPECT_EQ("[undefined]", GetExposedError(context));

  const char kRunWithLastError[] =
      "obj.runCallbackWithLastError('last error', function() {\n"
      "  this.exposedLastError =\n"
      "      this.lastError ? this.lastError.message : 'undefined';\n"
      "}, [1, 'foo']);";
  CallFunctionOnObject(context, v8_util, kRunWithLastError);

  EXPECT_FALSE(last_error()->HasError(context));
  EXPECT_EQ("[undefined]", GetExposedError(context));
  EXPECT_EQ("\"last error\"",
            GetStringPropertyFromObject(context->Global(), context,
                                        "exposedLastError"));
}

}  // namespace extensions
