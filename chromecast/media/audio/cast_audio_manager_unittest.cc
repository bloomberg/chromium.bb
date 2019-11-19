// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/common/mojom/multiroom.mojom.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/media/cma/test/mock_cma_backend.h"
#include "chromecast/media/cma/test/mock_cma_backend_factory.h"
#include "chromecast/media/cma/test/mock_multiroom_manager.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#if defined(OS_ANDROID)
#include "media/audio/android/audio_track_output_stream.h"
#endif  // defined(OS_ANDROID)

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::Return;
using testing::NiceMock;
using testing::StrictMock;

namespace {

const ::media::AudioParameters kDefaultAudioParams(
    ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
    ::media::CHANNEL_LAYOUT_STEREO,
    ::media::AudioParameters::kAudioCDSampleRate,
    256);

const ::media::AudioParameters kAudioParamsInvalidLayout(
    ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
    ::media::CHANNEL_LAYOUT_NONE,
    ::media::AudioParameters::kAudioCDSampleRate,
    256);

std::unique_ptr<service_manager::Connector> CreateConnector() {
  mojo::PendingReceiver<service_manager::mojom::Connector> receiver;
  return service_manager::Connector::Create(&receiver);
}

int OnMoreData(base::TimeDelta delay,
               base::TimeTicks delay_timestamp,
               int prior_frames_skipped,
               ::media::AudioBus* dest) {
  dest->Zero();
  return kDefaultAudioParams.frames_per_buffer();
}

std::string DummyGetSessionId(std::string /* audio_group_id */) {
  return "";
}

}  // namespace

namespace chromecast {
namespace media {

class CastAudioManagerTest : public testing::Test {
 public:
  CastAudioManagerTest() : audio_thread_("CastAudioThread") {}

  void SetUp() override { CreateAudioManagerForTesting(); }

  void TearDown() override {
    RunThreadsUntilIdle();
    audio_manager_->Shutdown();
    RunThreadsUntilIdle();
    audio_thread_.Stop();
  }

  // Binds |multiroom_manager_| to the interface requested through the test
  // connector.
  void BindMultiroomManager(mojo::ScopedMessagePipeHandle handle) {
    multiroom_manager_.Bind(std::move(handle));
  }

 protected:
  void FakeLogCallback(const std::string& string) {}
  CmaBackendFactory* GetCmaBackendFactory() {
    return mock_backend_factory_.get();
  }

  void CreateConnectorForTesting() {
    connector_ = CreateConnector();
    // Override the MultiroomManager interface for testing.
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(
            chromecast::mojom::kChromecastServiceName),
        mojom::MultiroomManager::Name_,
        base::BindRepeating(&CastAudioManagerTest::BindMultiroomManager,
                            base::Unretained(this)));
  }

  void CreateAudioManagerForTesting(bool use_mixer = false) {
    if (!connector_)
      CreateConnectorForTesting();

    // Only one AudioManager may exist at a time, so destroy the one we're
    // currently holding before creating a new one.
    // Flush the message loop to run any shutdown tasks posted by AudioManager.
    if (audio_manager_) {
      audio_manager_->Shutdown();
      audio_manager_.reset();
    }
    if (audio_thread_.IsRunning())
      audio_thread_.Stop();
    CHECK(audio_thread_.StartAndWaitForTesting());

    mock_backend_factory_ = std::make_unique<MockCmaBackendFactory>();
    audio_manager_ = base::WrapUnique(new CastAudioManager(
        std::make_unique<::media::TestAudioThread>(), &fake_audio_log_factory_,
        base::BindRepeating(&CastAudioManagerTest::GetCmaBackendFactory,
                            base::Unretained(this)),
        base::BindRepeating(&DummyGetSessionId),
        task_environment_.GetMainThreadTaskRunner(),
        audio_thread_.task_runner(), connector_.get(), use_mixer,
        true /* force_use_cma_backend_for_output*/
        ));
    // A few AudioManager implementations post initialization tasks to
    // audio thread. Flush the thread to ensure that |audio_manager_| is
    // initialized and ready to use before returning from this function.
    // TODO(alokp): We should perhaps do this in AudioManager::Create().
    RunThreadsUntilIdle();
    device_info_accessor_ =
        std::make_unique<::media::AudioDeviceInfoAccessorForTests>(
            audio_manager_.get());
  }

  void SetUpBackendAndDecoder() {
    mock_audio_decoder_ =
        std::make_unique<NiceMock<MockCmaBackend::AudioDecoder>>();
    EXPECT_CALL(*mock_audio_decoder_, SetDelegate(_)).Times(1);
    EXPECT_CALL(*mock_audio_decoder_, SetConfig(_)).WillOnce(Return(true));

    mock_cma_backend_ = std::make_unique<NiceMock<MockCmaBackend>>();
    EXPECT_CALL(*mock_cma_backend_, CreateAudioDecoder())
        .WillOnce(Return(mock_audio_decoder_.get()));
    EXPECT_CALL(*mock_cma_backend_, Initialize()).WillOnce(Return(true));

    EXPECT_CALL(*mock_backend_factory_, CreateBackend(_))
        .WillOnce(Invoke([this](const MediaPipelineDeviceParams&) {
          return std::move(mock_cma_backend_);
        }));
    EXPECT_EQ(mock_backend_factory_.get(),
              audio_manager_->cma_backend_factory());
  }

  void RunThreadsUntilIdle() {
    task_environment_.RunUntilIdle();
    audio_thread_.FlushForTesting();
  }

  void GetDefaultOutputStreamParameters(::media::AudioParameters* params) {
    *params = device_info_accessor_->GetDefaultOutputStreamParameters();
  }

  base::Thread audio_thread_;
  base::test::TaskEnvironment task_environment_;
  ::media::FakeAudioLogFactory fake_audio_log_factory_;
  std::unique_ptr<MockCmaBackendFactory> mock_backend_factory_;
  ::media::MockAudioSourceCallback mock_source_callback_;
  std::unique_ptr<MockCmaBackend> mock_cma_backend_;
  std::unique_ptr<MockCmaBackend::AudioDecoder> mock_audio_decoder_;

  std::unique_ptr<CastAudioManager> audio_manager_;
  std::unique_ptr<::media::AudioDeviceInfoAccessorForTests>
      device_info_accessor_;
  std::unique_ptr<service_manager::Connector> connector_;
  MockMultiroomManager multiroom_manager_;
};

TEST_F(CastAudioManagerTest, HasValidOutputStreamParameters) {
  ::media::AudioParameters params;
  GetDefaultOutputStreamParameters(&params);
  EXPECT_TRUE(params.IsValid());
}

TEST_F(CastAudioManagerTest, CanMakeStream) {
  SetUpBackendAndDecoder();
  ::media::AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      kDefaultAudioParams, "", ::media::AudioManager::LogCallback());
  EXPECT_TRUE(stream->Open());

  EXPECT_CALL(mock_source_callback_, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  EXPECT_CALL(mock_source_callback_, OnError(_)).Times(0);
  stream->Start(&mock_source_callback_);
  RunThreadsUntilIdle();

  stream->Stop();
  RunThreadsUntilIdle();

  stream->Close();
  RunThreadsUntilIdle();
}

#if defined(OS_ANDROID)
TEST_F(CastAudioManagerTest, CanMakeAC3Stream) {
  const ::media::AudioParameters kAC3AudioParams(
      ::media::AudioParameters::AUDIO_BITSTREAM_AC3,
      ::media::CHANNEL_LAYOUT_5_1, ::media::AudioParameters::kAudioCDSampleRate,
      256);
  ::media::AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      kAC3AudioParams, "", ::media::AudioManager::LogCallback());
  EXPECT_TRUE(stream);
  // Only run the rest of the test if the device supports AC3.
  if (stream->Open()) {
    EXPECT_CALL(mock_source_callback_, OnMoreData(_, _, _, _))
        .WillRepeatedly(Invoke(OnMoreData));
    EXPECT_CALL(mock_source_callback_, OnError(_)).Times(0);
    stream->Start(&mock_source_callback_);
    RunThreadsUntilIdle();

    stream->Stop();
    RunThreadsUntilIdle();
  }
  stream->Close();
}
#endif  // defined(OS_ANDROID)

TEST_F(CastAudioManagerTest, DISABLED_CanMakeStreamProxy) {
  SetUpBackendAndDecoder();
  ::media::AudioOutputStream* stream =
      audio_manager_->MakeAudioOutputStreamProxy(kDefaultAudioParams, "");
  EXPECT_TRUE(stream->Open());
  RunThreadsUntilIdle();
  EXPECT_CALL(mock_source_callback_, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  EXPECT_CALL(mock_source_callback_, OnError(_)).Times(0);
  stream->Start(&mock_source_callback_);
  RunThreadsUntilIdle();

  stream->Stop();

  stream->Close();
  RunThreadsUntilIdle();
  // TODO(steinbock) Figure out why stream is not unregistering itself from
  // audio_manager_
}

TEST_F(CastAudioManagerTest, CanMakeMixerStream) {
  CreateAudioManagerForTesting(true /* use_mixer */);
  SetUpBackendAndDecoder();
  ::media::AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      kDefaultAudioParams, "", ::media::AudioManager::LogCallback());
  EXPECT_TRUE(stream->Open());

  EXPECT_CALL(mock_source_callback_, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  EXPECT_CALL(mock_source_callback_, OnError(_)).Times(0);
  stream->Start(&mock_source_callback_);
  RunThreadsUntilIdle();

  stream->Stop();
  RunThreadsUntilIdle();

  stream->Close();
}

TEST_F(CastAudioManagerTest, CanMakeCommunicationsStream) {
  CreateAudioManagerForTesting();
  SetUpBackendAndDecoder();

  ::media::AudioOutputStream* stream = audio_manager_->MakeAudioOutputStream(
      kDefaultAudioParams,
      ::media::AudioDeviceDescription::kCommunicationsDeviceId,
      ::media::AudioManager::LogCallback());
  EXPECT_TRUE(stream->Open());

  EXPECT_CALL(mock_source_callback_, OnMoreData(_, _, _, _))
      .WillRepeatedly(Invoke(OnMoreData));
  EXPECT_CALL(mock_source_callback_, OnError(_)).Times(0);
  task_environment_.RunUntilIdle();

  stream->Stop();
  task_environment_.RunUntilIdle();

  stream->Close();
}

}  // namespace media
}  // namespace chromecast
