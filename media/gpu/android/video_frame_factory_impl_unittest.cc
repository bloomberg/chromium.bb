// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/config/gpu_preferences.h"
#include "media/gpu/android/maybe_render_early_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

struct MockMaybeRenderEarlyManager : public MaybeRenderEarlyManager {
  MOCK_METHOD1(SetSurfaceBundle, void(scoped_refptr<CodecSurfaceBundle>));
  MOCK_METHOD1(AddCodecImage, void(scoped_refptr<CodecImageHolder>));
  MOCK_METHOD0(MaybeRenderEarly, void());
};

class VideoFrameFactoryImplTest : public testing::Test {
 public:
  VideoFrameFactoryImplTest()
      : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~VideoFrameFactoryImplTest() override = default;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  gpu::GpuPreferences gpu_preferences_;
};

TEST_F(VideoFrameFactoryImplTest, CreateDoesntCrash) {
  // Placeholder test until we pull out the SharedImageVideoProvider.
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  std::unique_ptr<VideoFrameFactoryImpl> impl =
      std::make_unique<VideoFrameFactoryImpl>(
          task_runner_, get_stub_cb, gpu_preferences_,
          std::make_unique<MockMaybeRenderEarlyManager>());
}

}  // namespace media
