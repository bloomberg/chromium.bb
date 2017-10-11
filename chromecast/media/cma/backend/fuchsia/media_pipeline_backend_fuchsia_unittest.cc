// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/fuchsia/media_pipeline_backend_fuchsia.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class MediaPipelineBackendFuchsiaTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = std::make_unique<TaskRunnerImpl>();
    backend_ =
        std::make_unique<MediaPipelineBackendFuchsia>(MediaPipelineDeviceParams(
            task_runner_.get(), AudioContentType::kMedia,
            ::media::AudioDeviceDescription::kDefaultDeviceId));
  }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<TaskRunnerImpl> task_runner_;
  std::unique_ptr<MediaPipelineBackendFuchsia> backend_;
  MediaPipelineBackend::AudioDecoder* audio_decoder_ = nullptr;
};

TEST_F(MediaPipelineBackendFuchsiaTest, Initialize) {
  audio_decoder_ = backend_->CreateAudioDecoder();
  EXPECT_TRUE(audio_decoder_);
  EXPECT_TRUE(backend_->Initialize());
}

}  // namespace media
}  // namespace chromecast
