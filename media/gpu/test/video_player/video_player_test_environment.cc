// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player_test_environment.h"

#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video_player/video.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace media {
namespace test {
VideoPlayerTestEnvironment::VideoPlayerTestEnvironment(const Video* video)
    : video_(video) {}

void VideoPlayerTestEnvironment::SetUp() {
  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for the
  // test video player to function correctly.
  TestTimeouts::Initialize();
  task_environment_ = std::make_unique<base::test::ScopedTaskEnvironment>(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  // Perform all static initialization that is required when running video
  // decoders in a test environment.
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

#if defined(USE_OZONE)
  // Initialize Ozone. This is necessary to gain access to the GPU for hardware
  // video decode acceleration.
  LOG(WARNING) << "Initializing Ozone Platform...\n"
                  "If this hangs indefinitely please call 'stop ui' first!";
  ui::OzonePlatform::InitParams params = {.single_process = false};
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();

  // Initialize the Ozone GPU helper. If this is not done an error will occur:
  // "Check failed: drm. No devices available for buffer allocation."
  // Note: If a task environment is not set up initialization will hang
  // indefinitely here.
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif
}

void VideoPlayerTestEnvironment::TearDown() {
  task_environment_.reset();
}
}  // namespace test
}  // namespace media
