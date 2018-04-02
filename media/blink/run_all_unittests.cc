// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/platform/scheduler/web_main_thread_scheduler.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif

#if !defined(OS_IOS)
#include "mojo/edk/embedder/embedder.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

namespace {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif
#endif
}

class TestBlinkPlatformSupport : public blink::Platform {
 public:
  TestBlinkPlatformSupport()
      : renderer_scheduler_(
            blink::scheduler::CreateRendererSchedulerForTests()),
        main_thread_(renderer_scheduler_->CreateMainThread()) {}
  ~TestBlinkPlatformSupport() override;

  blink::WebThread* CurrentThread() override {
    EXPECT_TRUE(main_thread_->IsCurrentThread());
    return main_thread_.get();
  }

 private:
  std::unique_ptr<blink::scheduler::RendererScheduler> renderer_scheduler_;
  std::unique_ptr<blink::WebThread> main_thread_;
};

TestBlinkPlatformSupport::~TestBlinkPlatformSupport() {
  renderer_scheduler_->Shutdown();
}

class BlinkMediaTestSuite : public base::TestSuite {
 public:
  BlinkMediaTestSuite(int argc, char** argv);
  ~BlinkMediaTestSuite() override;

 protected:
  void Initialize() override;

 private:
  std::unique_ptr<TestBlinkPlatformSupport> blink_platform_support_;
};

BlinkMediaTestSuite::BlinkMediaTestSuite(int argc, char** argv)
    : TestSuite(argc, argv),
      blink_platform_support_(new TestBlinkPlatformSupport()) {
}

BlinkMediaTestSuite::~BlinkMediaTestSuite() = default;

void BlinkMediaTestSuite::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

#if defined(OS_ANDROID)
  if (media::MediaCodecUtil::IsMediaCodecAvailable())
    media::EnablePlatformDecoderSupport();
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibrary();

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
  gin::V8Initializer::LoadV8Natives();
#endif

// Initialize mojo firstly to enable Blink initialization to use it.
#if !defined(OS_IOS)
  mojo::edk::Init();
#endif
  // Dummy task runner is initialized here because the blink::initialize creates
  // IsolateHolder which needs the current task runner handle. There should be
  // no task posted to this task runner.
  std::unique_ptr<base::MessageLoop> message_loop;
  if (!base::MessageLoop::current())
    message_loop.reset(new base::MessageLoop());
  service_manager::BinderRegistry empty_registry;
  blink::Initialize(blink_platform_support_.get(), &empty_registry);
}

int main(int argc, char** argv) {
  BlinkMediaTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&BlinkMediaTestSuite::Run,
                             base::Unretained(&test_suite)));
}
