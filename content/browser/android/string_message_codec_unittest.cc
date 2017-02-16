// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/string_message_codec.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_async_task_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace content {
namespace {

base::string16 DecodeWithV8(const base::string16& encoded) {
  base::test::ScopedAsyncTaskScheduler task_scheduler;
  base::string16 result;

  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(params);
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Context> context = v8::Context::New(isolate);

    v8::ValueDeserializer deserializer(
        isolate,
        reinterpret_cast<const uint8_t*>(encoded.data()),
        encoded.size() * sizeof(base::char16));
    deserializer.SetSupportsLegacyWireFormat(true);

    EXPECT_TRUE(deserializer.ReadHeader(context).ToChecked());

    v8::Local<v8::Value> value =
        deserializer.ReadValue(context).ToLocalChecked();
    v8::Local<v8::String> str = value->ToString(context).ToLocalChecked();

    result.resize(str->Length());
    str->Write(&result[0], 0, result.size());
  }
  isolate->Dispose();
  delete params.array_buffer_allocator;

  return result;
}

base::string16 EncodeWithV8(const base::string16& message) {
  base::test::ScopedAsyncTaskScheduler task_scheduler;
  base::string16 result;

  v8::Isolate::CreateParams params;
  params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(params);
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Context> context = v8::Context::New(isolate);

    v8::Local<v8::String> message_as_value =
        v8::String::NewFromTwoByte(isolate,
                                   message.data(),
                                   v8::NewStringType::kNormal,
                                   message.size()).ToLocalChecked();

    v8::ValueSerializer serializer(isolate);
    serializer.WriteHeader();
    EXPECT_TRUE(serializer.WriteValue(context, message_as_value).ToChecked());

    std::pair<uint8_t*, size_t> buffer = serializer.Release();

    size_t result_num_bytes = (buffer.second + 1) & ~1;
    result.resize(result_num_bytes / 2);
    memcpy(reinterpret_cast<uint8_t*>(&result[0]), buffer.first, buffer.second);

    free(buffer.first);
  }
  isolate->Dispose();
  delete params.array_buffer_allocator;

  return result;
}

TEST(StringMessageCodecTest, SelfTest_ASCII) {
  base::string16 message = base::ASCIIToUTF16("hello");
  base::string16 decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfTest_NonASCII) {
  base::string16 message = base::WideToUTF16(L"hello \u263A");
  base::string16 decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfTest_NonASCIILongEnoughToForcePadding) {
  base::string16 message(200, 0x263A);
  base::string16 decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeStringMessage(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, SelfToV8Test_ASCII) {
  base::string16 message = base::ASCIIToUTF16("hello");
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, SelfToV8Test_NonASCII) {
  base::string16 message = base::WideToUTF16(L"hello \u263A");
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, SelfToV8Test_NonASCIILongEnoughToForcePadding) {
  base::string16 message(200, 0x263A);
  EXPECT_EQ(message, DecodeWithV8(EncodeStringMessage(message)));
}

TEST(StringMessageCodecTest, V8ToSelfTest_ASCII) {
  base::string16 message = base::ASCIIToUTF16("hello");
  base::string16 decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, V8ToSelfTest_NonASCII) {
  base::string16 message = base::WideToUTF16(L"hello \u263A");
  base::string16 decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

TEST(StringMessageCodecTest, V8ToSelfTest_NonASCIILongEnoughToForcePadding) {
  base::string16 message(200, 0x263A);
  base::string16 decoded;
  EXPECT_TRUE(DecodeStringMessage(EncodeWithV8(message), &decoded));
  EXPECT_EQ(message, decoded);
}

}  // namespace
}  // namespace content
