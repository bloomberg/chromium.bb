// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/physical_web/eddystone_encoder_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

class EddystoneEncoderBridgeTest : public testing::Test {
 public:
  EddystoneEncoderBridgeTest() {}
  ~EddystoneEncoderBridgeTest() override {}

  void SetUp() override { env_ = base::android::AttachCurrentThread(); }

  void TearDown() override {}

  JNIEnv* Env();
  jstring JavaString(const std::string& value);
  bool ScopedJavaLocalRefJByteArrayEquals(JNIEnv* env,
                                          ScopedJavaLocalRef<jbyteArray> a,
                                          ScopedJavaLocalRef<jbyteArray> b);

 private:
  JNIEnv* env_;
};

JNIEnv* EddystoneEncoderBridgeTest::Env() {
  return env_;
}

jstring EddystoneEncoderBridgeTest::JavaString(const std::string& value) {
  return base::android::ConvertUTF8ToJavaString(Env(), value).Release();
}

bool EddystoneEncoderBridgeTest::ScopedJavaLocalRefJByteArrayEquals(
    JNIEnv* env,
    ScopedJavaLocalRef<jbyteArray> a,
    ScopedJavaLocalRef<jbyteArray> b) {
  jbyteArray a_byte_array = a.obj();
  jbyteArray b_byte_array = b.obj();

  int a_len = env->GetArrayLength(a_byte_array);
  if (a_len != env->GetArrayLength(b_byte_array)) {
    return false;
  }

  jbyte* a_elems = env->GetByteArrayElements(a_byte_array, NULL);
  jbyte* b_elems = env->GetByteArrayElements(b_byte_array, NULL);

  bool cmp = memcmp(a_elems, b_elems, a_len) == 0;

  env->ReleaseByteArrayElements(a_byte_array, a_elems, JNI_ABORT);
  env->ReleaseByteArrayElements(b_byte_array, b_elems, JNI_ABORT);

  return cmp;
}

TEST_F(EddystoneEncoderBridgeTest, TestNullUrl) {
  ScopedJavaLocalRef<jbyteArray> actual =
      EncodeUrlForTesting(Env(), JavaParamRef<jobject>(nullptr),
                          JavaParamRef<jstring>(Env(), nullptr));
  EXPECT_TRUE(Env()->ExceptionCheck());
}

TEST_F(EddystoneEncoderBridgeTest, TestInvalidUrl) {
  std::string url = "Wrong!";
  ScopedJavaLocalRef<jbyteArray> actual =
      EncodeUrlForTesting(Env(), JavaParamRef<jobject>(nullptr),
                          JavaParamRef<jstring>(Env(), JavaString(url)));
  EXPECT_TRUE(Env()->ExceptionCheck());
}

TEST_F(EddystoneEncoderBridgeTest, TestValidUrl) {
  std::string url = "https://www.example.com/";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x00};
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);
  ScopedJavaLocalRef<jbyteArray> expected =
      ToJavaByteArray(Env(), &expected_array[0], expected_array_length);
  ScopedJavaLocalRef<jbyteArray> actual =
      EncodeUrlForTesting(Env(), JavaParamRef<jobject>(nullptr),
                          JavaParamRef<jstring>(Env(), JavaString(url)));
  EXPECT_TRUE(ScopedJavaLocalRefJByteArrayEquals(Env(), expected, actual));
  EXPECT_FALSE(Env()->ExceptionCheck());
}
