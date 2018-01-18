// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_alsa.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/test_message_loop.h"
#include "chromecast/media/cma/test/mock_media_pipeline_backend_factory.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace chromecast {
namespace media {
namespace {

const char kDefaultAlsaDevice[] = "plug:default";

const ::media::AudioParameters kDefaultAudioParams(
    ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
    ::media::CHANNEL_LAYOUT_STEREO,
    ::media::AudioParameters::kAudioCDSampleRate,
    16,
    256);

void OnLogMessage(const std::string& message) {}

class CastAudioManagerAlsaTest : public testing::Test {
 public:
  CastAudioManagerAlsaTest() : media_thread_("CastMediaThread") {
    CHECK(media_thread_.Start());

    backend_factory_ = new MockMediaPipelineBackendFactory();
    audio_manager_ = std::make_unique<CastAudioManagerAlsa>(
        std::make_unique<::media::TestAudioThread>(), &audio_log_factory_,
        base::WrapUnique(backend_factory_), media_thread_.task_runner(), false);
  }

  ~CastAudioManagerAlsaTest() override { audio_manager_->Shutdown(); }

 protected:
  base::TestMessageLoop message_loop_;
  base::Thread media_thread_;
  ::media::FakeAudioLogFactory audio_log_factory_;
  std::unique_ptr<CastAudioManagerAlsa> audio_manager_;

  // Owned by |audio_manager_|
  MockMediaPipelineBackendFactory* backend_factory_;
};

TEST_F(CastAudioManagerAlsaTest, MakeAudioInputStream) {
  ::media::AudioInputStream* stream = audio_manager_->MakeAudioInputStream(
      kDefaultAudioParams, kDefaultAlsaDevice, base::Bind(&OnLogMessage));
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());
  stream->Close();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
