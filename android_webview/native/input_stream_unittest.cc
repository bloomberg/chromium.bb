// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/input_stream_impl.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "jni/InputStreamUnittest_jni.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using android_webview::InputStream;
using android_webview::InputStreamImpl;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using net::IOBuffer;
using testing::DoAll;
using testing::Ge;
using testing::InSequence;
using testing::Lt;
using testing::Ne;
using testing::NotNull;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

class InputStreamTest : public Test {
 public:
  InputStreamTest() {
  }
 protected:
  virtual void SetUp() {
    env_ = AttachCurrentThread();
    ASSERT_THAT(env_, NotNull());
    ASSERT_TRUE(android_webview::RegisterInputStream(env_));
    ASSERT_TRUE(RegisterNativesImpl(env_));
  }

  scoped_refptr<IOBuffer> DoReadCountedStreamTest(int streamSize,
                                                  int bytesToRead) {
    ScopedJavaLocalRef<jobject> counting_jstream =
        Java_InputStreamUnittest_getCountingStream(env_, streamSize);
    EXPECT_FALSE(counting_jstream.is_null());

    scoped_ptr<InputStream> input_stream(
        new InputStreamImpl(counting_jstream));
    int bytesRead = 0;
    scoped_refptr<IOBuffer> buffer = new IOBuffer(bytesToRead);

    EXPECT_TRUE(input_stream->Read(buffer.get(), bytesToRead, &bytesRead));
    EXPECT_EQ(bytesToRead, bytesRead);
    return buffer;
  }

  JNIEnv* env_;
};

TEST_F(InputStreamTest, ReadEmptyStream) {
  ScopedJavaLocalRef<jobject> empty_jstream =
      Java_InputStreamUnittest_getEmptyStream(env_);
  EXPECT_FALSE(empty_jstream.is_null());

  scoped_ptr<InputStream> input_stream(new InputStreamImpl(empty_jstream));
  const int bytesToRead = 10;
  int bytesRead = 0;
  scoped_refptr<IOBuffer> buffer = new IOBuffer(bytesToRead);

  EXPECT_TRUE(input_stream->Read(buffer.get(), bytesToRead, &bytesRead));
  EXPECT_EQ(0, bytesRead);
}

TEST_F(InputStreamTest, ReadStreamPartial) {
  const int bytesToRead = 128;
  DoReadCountedStreamTest(bytesToRead * 2, bytesToRead);
}

TEST_F(InputStreamTest, ReadStreamCompletely) {
  const int bytesToRead = 42;
  DoReadCountedStreamTest(bytesToRead, bytesToRead);
}

TEST_F(InputStreamTest, ReadStreamInChunks) {
  const int bytesToRead = 3 * InputStreamImpl::kBufferSize;
  DoReadCountedStreamTest(bytesToRead, bytesToRead);
}

TEST_F(InputStreamTest, CheckContentsReadCorrectly) {
  const int bytesToRead = 256;
  scoped_refptr<IOBuffer> buffer =
      DoReadCountedStreamTest(bytesToRead, bytesToRead);
  for (int i = 0; i < bytesToRead; ++i) {
    EXPECT_EQ(i, buffer->data()[i]);
  }
}
