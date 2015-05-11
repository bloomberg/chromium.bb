// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/base/android/media_jni_registrar.h"
#include "ui/gl/android/gl_jni_registrar.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

class TestBlinkPlatformSupport : NON_EXPORTED_BASE(public blink::Platform) {
 public:
  virtual ~TestBlinkPlatformSupport();

  virtual void cryptographicallyRandomValues(unsigned char* buffer,
                                             size_t length) override;
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* categoryName) override;
};

TestBlinkPlatformSupport::~TestBlinkPlatformSupport() {}

void TestBlinkPlatformSupport::cryptographicallyRandomValues(
    unsigned char* buffer,
    size_t length) {
}

const unsigned char* TestBlinkPlatformSupport::getTraceCategoryEnabledFlag(
    const char* categoryName) {
  static const unsigned char tracingIsDisabled = 0;
  return &tracingIsDisabled;
}

class BlinkMediaTestSuite : public base::TestSuite {
 public:
  BlinkMediaTestSuite(int argc, char** argv);
  ~BlinkMediaTestSuite() override;

 protected:
  void Initialize() override;

 private:
  scoped_ptr<TestBlinkPlatformSupport> blink_platform_support_;
};

BlinkMediaTestSuite::BlinkMediaTestSuite(int argc, char** argv)
    : TestSuite(argc, argv),
      blink_platform_support_(new TestBlinkPlatformSupport()) {
}

BlinkMediaTestSuite::~BlinkMediaTestSuite() {}

void BlinkMediaTestSuite::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  JNIEnv* env = base::android::AttachCurrentThread();
  // Needed for surface texture support.
  ui::gl::android::RegisterJni(env);
  media::RegisterJni(env);
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibraryForTesting();

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif

  // Dummy task runner is initialized here because the blink::initialize creates
  // IsolateHolder which needs the current task runner handle. There should be
  // no task posted to this task runner.
  scoped_ptr<base::MessageLoop> message_loop;
  if (!base::MessageLoop::current())
    message_loop.reset(new base::MessageLoop());
  blink::initialize(blink_platform_support_.get());
}

int main(int argc, char** argv) {
  BlinkMediaTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&BlinkMediaTestSuite::Run,
                             base::Unretained(&test_suite)));
}
