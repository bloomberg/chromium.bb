// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kSamplesPerPacket = kSampleRate / 100;
static const double kTestVolume = 0.25;

class MockAudioOutputControllerEventHandler
    : public AudioOutputController::EventHandler {
 public:
  MockAudioOutputControllerEventHandler() {}

  MOCK_METHOD0(OnCreated, void());
  MOCK_METHOD0(OnPlaying, void());
  MOCK_METHOD0(OnPaused, void());
  MOCK_METHOD0(OnError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerEventHandler);
};

class MockAudioOutputControllerSyncReader
    : public AudioOutputController::SyncReader {
 public:
  MockAudioOutputControllerSyncReader() {}

  MOCK_METHOD2(UpdatePendingBytes,
               void(uint32_t bytes, uint32_t frames_skipped));
  MOCK_METHOD1(Read, void(AudioBus* dest));
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerSyncReader);
};

class MockAudioOutputStream : public AudioOutputStream {
 public:
  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioSourceCallback* callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());

  // Set/get the callback passed to Start().
  AudioSourceCallback* callback() const { return callback_; }
  void SetCallback(AudioSourceCallback* asc) { callback_ = asc; }

 private:
  AudioSourceCallback* callback_;
};

static const float kBufferNonZeroData = 1.0f;
ACTION(PopulateBuffer) {
  arg0->Zero();
  // Note: To confirm the buffer will be populated in these tests, it's
  // sufficient that only the first float in channel 0 is set to the value.
  arg0->channel(0)[0] = kBufferNonZeroData;
}

class AudioOutputControllerTest : public testing::Test {
 public:
  AudioOutputControllerTest()
      : audio_manager_(AudioManager::CreateForTesting(
            base::ThreadTaskRunnerHandle::Get())) {
    base::RunLoop().RunUntilIdle();
  }

  ~AudioOutputControllerTest() override {}

 protected:
  void Create(int samples_per_packet) {
    params_ = AudioParameters(
        AudioParameters::AUDIO_FAKE, kChannelLayout,
        kSampleRate, kBitsPerSample, samples_per_packet);

    if (params_.IsValid()) {
      EXPECT_CALL(mock_event_handler_, OnCreated());
    }

    controller_ = AudioOutputController::Create(
        audio_manager_.get(), &mock_event_handler_, params_, std::string(),
        &mock_sync_reader_);
    if (controller_.get())
      controller_->SetVolume(kTestVolume);

    EXPECT_EQ(params_.IsValid(), controller_.get() != NULL);
    base::RunLoop().RunUntilIdle();
  }

  void Play() {
    // Expect the event handler to receive one OnPlaying() call.
    EXPECT_CALL(mock_event_handler_, OnPlaying());

    // During playback, the mock pretends to provide audio data rendered and
    // sent from the render process.
    EXPECT_CALL(mock_sync_reader_, UpdatePendingBytes(_, _)).Times(AtLeast(1));
    EXPECT_CALL(mock_sync_reader_, Read(_)).WillRepeatedly(PopulateBuffer());
    controller_->Play();
    base::RunLoop().RunUntilIdle();
  }

  void Pause() {
    // Expect the event handler to receive one OnPaused() call.
    EXPECT_CALL(mock_event_handler_, OnPaused());

    controller_->Pause();
    base::RunLoop().RunUntilIdle();
  }

  void ChangeDevice() {
    // Expect the event handler to receive one OnPaying() call and no OnPaused()
    // call.
    EXPECT_CALL(mock_event_handler_, OnPlaying());
    EXPECT_CALL(mock_event_handler_, OnPaused())
        .Times(0);

    // Simulate a device change event to AudioOutputController from the
    // AudioManager.
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&AudioOutputController::OnDeviceChange, controller_));
  }

  void Divert(bool was_playing, int num_times_to_be_started) {
    if (was_playing) {
      // Expect the handler to receive one OnPlaying() call as a result of the
      // stream switching.
      EXPECT_CALL(mock_event_handler_, OnPlaying());
    }

    EXPECT_CALL(mock_stream_, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume));
    if (num_times_to_be_started > 0) {
      EXPECT_CALL(mock_stream_, Start(NotNull()))
          .Times(num_times_to_be_started)
          .WillRepeatedly(
              Invoke(&mock_stream_, &MockAudioOutputStream::SetCallback));
      EXPECT_CALL(mock_stream_, Stop())
          .Times(num_times_to_be_started);
    }

    controller_->StartDiverting(&mock_stream_);
    base::RunLoop().RunUntilIdle();
  }

  void ReadDivertedAudioData() {
    scoped_ptr<AudioBus> dest = AudioBus::Create(params_);
    ASSERT_TRUE(mock_stream_.callback());
    const int frames_read =
        mock_stream_.callback()->OnMoreData(dest.get(), 0, 0);
    EXPECT_LT(0, frames_read);
    EXPECT_EQ(kBufferNonZeroData, dest->channel(0)[0]);
  }

  void Revert(bool was_playing) {
    if (was_playing) {
      // Expect the handler to receive one OnPlaying() call as a result of the
      // stream switching back.
      EXPECT_CALL(mock_event_handler_, OnPlaying());
    }

    EXPECT_CALL(mock_stream_, Close());

    controller_->StopDiverting();
    base::RunLoop().RunUntilIdle();
  }

  void SwitchDevice(bool diverting) {
    if (!diverting) {
      // Expect the current stream to close and a new stream to start
      // playing if not diverting. When diverting, nothing happens
      // until diverting is stopped.
      EXPECT_CALL(mock_event_handler_, OnPlaying());
    }

    controller_->SwitchOutputDevice(AudioManager::GetDefaultDeviceName(),
                                    base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
  }

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());

    base::RunLoop run_loop;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&AudioOutputController::Close, controller_,
                              run_loop.QuitClosure()));
    run_loop.Run();
  }

  // These help make test sequences more readable.
  void DivertNeverPlaying() { Divert(false, 0); }
  void DivertWillEventuallyBeTwicePlayed() { Divert(false, 2); }
  void DivertWhilePlaying() { Divert(true, 1); }
  void RevertWasNotPlaying() { Revert(false); }
  void RevertWhilePlaying() { Revert(true); }

 private:
  base::TestMessageLoop message_loop_;
  ScopedAudioManagerPtr audio_manager_;
  MockAudioOutputControllerEventHandler mock_event_handler_;
  MockAudioOutputControllerSyncReader mock_sync_reader_;
  MockAudioOutputStream mock_stream_;
  AudioParameters params_;
  scoped_refptr<AudioOutputController> controller_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputControllerTest);
};

TEST_F(AudioOutputControllerTest, CreateAndClose) {
  Create(kSamplesPerPacket);
  Close();
}

TEST_F(AudioOutputControllerTest, HardwareBufferTooLarge) {
  Create(kSamplesPerPacket * 1000);
}

TEST_F(AudioOutputControllerTest, PlayAndClose) {
  Create(kSamplesPerPacket);
  Play();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayPauseClose) {
  Create(kSamplesPerPacket);
  Play();
  Pause();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayPausePlayClose) {
  Create(kSamplesPerPacket);
  Play();
  Pause();
  Play();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDeviceChangeClose) {
  Create(kSamplesPerPacket);
  Play();
  ChangeDevice();
  Close();
}

TEST_F(AudioOutputControllerTest, PlaySwitchDeviceClose) {
  Create(kSamplesPerPacket);
  Play();
  SwitchDevice(false);
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDivertRevertClose) {
  Create(kSamplesPerPacket);
  Play();
  DivertWhilePlaying();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDivertSwitchDeviceRevertClose) {
  Create(kSamplesPerPacket);
  Play();
  DivertWhilePlaying();
  SwitchDevice(true);
  ReadDivertedAudioData();
  RevertWhilePlaying();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDivertRevertDivertRevertClose) {
  Create(kSamplesPerPacket);
  Play();
  DivertWhilePlaying();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  DivertWhilePlaying();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  Close();
}

TEST_F(AudioOutputControllerTest, DivertPlayPausePlayRevertClose) {
  Create(kSamplesPerPacket);
  DivertWillEventuallyBeTwicePlayed();
  Play();
  ReadDivertedAudioData();
  Pause();
  Play();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  Close();
}

TEST_F(AudioOutputControllerTest, DivertRevertClose) {
  Create(kSamplesPerPacket);
  DivertNeverPlaying();
  RevertWasNotPlaying();
  Close();
}

}  // namespace media
