// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player_test_environment.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
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

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("test-25fps.h264");

// static
VideoPlayerTestEnvironment* VideoPlayerTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    bool enable_validator,
    bool output_frames,
    bool use_vd) {
  auto video = std::make_unique<media::test::Video>(
      video_path.empty() ? base::FilePath(kDefaultTestVideoPath) : video_path,
      video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  return new VideoPlayerTestEnvironment(std::move(video), enable_validator,
                                        output_frames, use_vd);
}

VideoPlayerTestEnvironment::VideoPlayerTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_validator,
    bool output_frames,
    bool use_vd)
    : video_(std::move(video)),
      enable_validator_(enable_validator),
      output_frames_(output_frames),
      use_vd_(use_vd) {}

VideoPlayerTestEnvironment::~VideoPlayerTestEnvironment() = default;

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

const media::test::Video* VideoPlayerTestEnvironment::Video() const {
  return video_.get();
}

bool VideoPlayerTestEnvironment::IsValidatorEnabled() const {
  return enable_validator_;
}

bool VideoPlayerTestEnvironment::IsFramesOutputEnabled() const {
  return output_frames_;
}

bool VideoPlayerTestEnvironment::UseVD() const {
  return use_vd_;
}

base::FilePath::StringType VideoPlayerTestEnvironment::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
#if defined(OS_WIN)
  // On Windows the default file path string type is UTF16. Since the test name
  // is always returned in UTF8 we need to do a conversion here.
  return base::UTF8ToUTF16(test_info->name());
#else
  return test_info->name();
#endif
}

}  // namespace test
}  // namespace media
