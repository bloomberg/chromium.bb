// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_last_error.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"

namespace extensions {

namespace {

std::string GetLastError(v8::Local<v8::Object> parent,
                         v8::Local<v8::Context> context) {
  v8::Local<v8::Value> last_error =
      GetPropertyFromObject(parent, context, "lastError");
  if (last_error.IsEmpty() || !last_error->IsObject())
    return V8ToString(last_error, context);
  v8::Local<v8::Value> message =
      GetPropertyFromObject(last_error.As<v8::Object>(), context, "message");
  return V8ToString(message, context);
}

using ParentList =
    std::vector<std::pair<v8::Local<v8::Context>, v8::Local<v8::Object>>>;
v8::Local<v8::Object> GetParent(const ParentList& parents,
                                v8::Local<v8::Context> context) {
  // This would be simpler with a map<context, object>, but Local<> doesn't
  // define an operator<.
  for (const auto& parent : parents) {
    if (parent.first == context)
      return parent.second;
  }
  return v8::Local<v8::Object>();
}

}  // namespace

using APILastErrorTest = APIBindingTest;

// Test basic functionality of the lastError object.
TEST_F(APILastErrorTest, TestLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  ParentList parents = {{context, parent_object}};
  APILastError last_error(base::Bind(&GetParent, parents));

  EXPECT_EQ("undefined", GetLastError(parent_object, context));
  // Check that the key isn't present on the object (as opposed to simply being
  // undefined).
  EXPECT_FALSE(
      parent_object->Has(context, gin::StringToV8(isolate(), "lastError"))
          .ToChecked());

  last_error.SetError(context, "Some last error");
  EXPECT_EQ("\"Some last error\"", GetLastError(parent_object, context));

  last_error.ClearError(context, false);
  EXPECT_EQ("undefined", GetLastError(parent_object, context));
  EXPECT_FALSE(
      parent_object->Has(context, gin::StringToV8(isolate(), "lastError"))
          .ToChecked());
}

// Test throwing an error if the last error wasn't checked.
TEST_F(APILastErrorTest, ReportIfUnchecked) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  ParentList parents = {{context, parent_object}};
  APILastError last_error(base::Bind(&GetParent, parents));

  {
    v8::TryCatch try_catch(isolate());
    last_error.SetError(context, "foo");
    // GetLastError() will count as accessing the error property, so we
    // shouldn't throw an exception.
    EXPECT_EQ("\"foo\"", GetLastError(parent_object, context));
    last_error.ClearError(context, true);
    EXPECT_FALSE(try_catch.HasCaught());
  }

  {
    v8::TryCatch try_catch(isolate());
    // This time, we should throw an exception.
    last_error.SetError(context, "A last error");
    last_error.ClearError(context, true);
    ASSERT_TRUE(try_catch.HasCaught());
    EXPECT_EQ("Uncaught Error: A last error",
              gin::V8ToString(try_catch.Message()->Get()));
  }
}

// Test behavior when something else sets a lastError property on the parent
// object.
TEST_F(APILastErrorTest, NonLastErrorObject) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  ParentList parents = {{context, parent_object}};
  APILastError last_error(base::Bind(&GetParent, parents));

  auto checked_set = [context](v8::Local<v8::Object> object,
                               base::StringPiece key,
                               v8::Local<v8::Value> value) {
    v8::Maybe<bool> success = object->Set(
        context, gin::StringToSymbol(context->GetIsolate(), key), value);
    ASSERT_TRUE(success.IsJust());
    ASSERT_TRUE(success.FromJust());
  };

  // Set a "fake" lastError property on the parent.
  v8::Local<v8::Object> fake_last_error = v8::Object::New(isolate());
  checked_set(fake_last_error, "message",
              gin::StringToV8(isolate(), "fake error"));
  checked_set(parent_object, "lastError", fake_last_error);

  EXPECT_EQ("\"fake error\"", GetLastError(parent_object, context));

  // The bindings shouldn't mangle an existing property (or maybe we should -
  // see the TODO in api_last_error.cc).
  last_error.SetError(context, "Real last error");
  EXPECT_EQ("\"fake error\"", GetLastError(parent_object, context));
  last_error.ClearError(context, false);
  EXPECT_EQ("\"fake error\"", GetLastError(parent_object, context));
}

// Test lastError in multiple different contexts.
TEST_F(APILastErrorTest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = ContextLocal();
  v8::Local<v8::Context> context_b = v8::Context::New(isolate());
  gin::ContextHolder holder_b(isolate());
  holder_b.SetContext(context_b);

  v8::Local<v8::Object> parent_a = v8::Object::New(isolate());
  v8::Local<v8::Object> parent_b = v8::Object::New(isolate());
  ParentList parents = {{context_a, parent_a}, {context_b, parent_b}};
  APILastError last_error(base::Bind(&GetParent, parents));

  last_error.SetError(context_a, "Last error a");
  EXPECT_EQ("\"Last error a\"", GetLastError(parent_a, context_a));
  EXPECT_EQ("undefined", GetLastError(parent_b, context_b));

  last_error.SetError(context_b, "Last error b");
  EXPECT_EQ("\"Last error a\"", GetLastError(parent_a, context_a));
  EXPECT_EQ("\"Last error b\"", GetLastError(parent_b, context_b));

  last_error.ClearError(context_b, false);
  EXPECT_EQ("\"Last error a\"", GetLastError(parent_a, context_a));
  EXPECT_EQ("undefined", GetLastError(parent_b, context_b));

  last_error.ClearError(context_a, false);
  EXPECT_EQ("undefined", GetLastError(parent_a, context_a));
  EXPECT_EQ("undefined", GetLastError(parent_b, context_b));
}

}  // namespace extensions
