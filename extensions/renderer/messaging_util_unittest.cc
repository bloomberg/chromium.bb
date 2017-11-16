// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_util.h"

#include <memory>

#include "extensions/common/api/messaging/message.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
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

}  // namespace extensions
