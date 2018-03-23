// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_util.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/extension_builder.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "v8/include/v8.h"

namespace extensions {

using MessagingUtilTest = APIBindingTest;

TEST_F(MessagingUtilTest, TestMaximumMessageSize) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  constexpr char kMessageTooLongError[] =
      "Message length exceeded maximum allowed length.";

  {
    v8::Local<v8::Value> long_message =
        V8ValueFromScriptSource(context, "'a'.repeat(1024 *1024 * 65)");
    std::string error;
    std::unique_ptr<Message> message =
        messaging_util::MessageFromV8(context, long_message, &error);
    EXPECT_FALSE(message);
    EXPECT_EQ(kMessageTooLongError, error);
  }

  {
    v8::Local<v8::Value> long_json_message = V8ValueFromScriptSource(
        context, "(JSON.stringify('a'.repeat(1024 *1024 * 65)))");
    ASSERT_TRUE(long_json_message->IsString());
    std::string error;
    std::unique_ptr<Message> message = messaging_util::MessageFromV8(
        context, long_json_message.As<v8::String>(), &error);
    EXPECT_FALSE(message);
    EXPECT_EQ(kMessageTooLongError, error);
  }
}

using MessagingUtilWithSystemTest = NativeExtensionBindingsSystemUnittest;

TEST_F(MessagingUtilWithSystemTest, TestGetTargetIdFromExtensionContext) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  scoped_refptr<Extension> extension = ExtensionBuilder("foo").Build();
  RegisterExtension(extension);

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  std::string other_id(32, 'a');
  struct {
    v8::Local<v8::Value> passed_id;
    base::StringPiece expected_id;
    bool should_pass;
  } test_cases[] = {
      // If the extension ID is not provided, the bindings use the calling
      // extension's.
      {v8::Null(isolate()), extension->id(), true},
      // We treat the empty string to be the same as null, even though it's
      // somewhat unfortunate.
      // See https://crbug.com/823577.
      {gin::StringToV8(isolate(), ""), extension->id(), true},
      {gin::StringToV8(isolate(), extension->id()), extension->id(), true},
      {gin::StringToV8(isolate(), other_id), other_id, true},
      {gin::StringToV8(isolate(), "invalid id"), base::StringPiece(), false},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test Case: %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];
    std::string target;
    std::string error;
    EXPECT_EQ(test_case.should_pass,
              messaging_util::GetTargetExtensionId(
                  script_context, test_case.passed_id, "runtime.sendMessage",
                  &target, &error));
    EXPECT_EQ(test_case.expected_id, target);
    EXPECT_EQ(test_case.should_pass, error.empty()) << error;
  }
}

TEST_F(MessagingUtilWithSystemTest, TestGetTargetIdFromWebContext) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context =
      CreateScriptContext(context, nullptr, Feature::WEB_PAGE_CONTEXT);
  script_context->set_url(GURL("https://example.com"));

  std::string other_id(32, 'a');
  struct {
    v8::Local<v8::Value> passed_id;
    base::StringPiece expected_id;
    bool should_pass;
  } test_cases[] = {
      // A web page should always have to specify the extension id.
      {gin::StringToV8(isolate(), other_id), other_id, true},
      {v8::Null(isolate()), base::StringPiece(), false},
      {gin::StringToV8(isolate(), ""), base::StringPiece(), false},
      {gin::StringToV8(isolate(), "invalid id"), base::StringPiece(), false},
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test Case: %d", static_cast<int>(i)));
    const auto& test_case = test_cases[i];
    std::string target;
    std::string error;
    EXPECT_EQ(test_case.should_pass,
              messaging_util::GetTargetExtensionId(
                  script_context, test_case.passed_id, "runtime.sendMessage",
                  &target, &error));
    EXPECT_EQ(test_case.expected_id, target);
    EXPECT_EQ(test_case.should_pass, error.empty()) << error;
  }
}

}  // namespace extensions
