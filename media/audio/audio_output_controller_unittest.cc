// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
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
static const int kHardwareBufferSize = kSamplesPerPacket *
    ChannelLayoutToChannelCount(kChannelLayout) * kBitsPerSample / 8;
static const double kTestVolume = 0.25;

class MockAudioOutputControllerEventHandler
    : public AudioOutputController::EventHandler {
 public:
  MockAudioOutputControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioOutputController* controller));
  MOCK_METHOD1(OnPlaying, void(AudioOutputController* controller));
  MOCK_METHOD1(OnPaused, void(AudioOutputController* controller));
  MOCK_METHOD2(OnError, void(AudioOutputController* controller,
                             int error_code));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerEventHandler);
};

class MockAudioOutputControllerSyncReader
    : public AudioOutputController::SyncReader {
 public:
  MockAudioOutputControllerSyncReader() {}

  MOCK_METHOD1(UpdatePendingBytes, void(uint32 bytes));
  MOCK_METHOD2(Read, int(AudioBus* source, AudioBus* dest));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(DataReady, bool());

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

ACTION_P(SignalEvent, event) {
  event->Signal();
}

static const float kBufferNonZeroData = 1.0f;
ACTION(PopulateBuffer) {
  arg1->Zero();
  // Note: To confirm the buffer will be populated in these tests, it's
  // sufficient that only the first float in channel 0 is set to the value.
  arg1->channel(0)[0] = kBufferNonZeroData;
}

class AudioOutputControllerTest : public testing::Test {
 public:
  AudioOutputControllerTest()
      : audio_manager_(AudioManager::Create()),
        create_event_(false, false),
        play_event_(false, false),
        read_event_(false, false),
        pause_event_(false, false) {
  }

  virtual ~AudioOutputControllerTest() {
  }

 protected:
  void Create(int samples_per_packet) {
    EXPECT_FALSE(create_event_.IsSignaled());
    EXPECT_FALSE(play_event_.IsSignaled());
    EXPECT_FALSE(read_event_.IsSignaled());
    EXPECT_FALSE(pause_event_.IsSignaled());

    params_ = AudioParameters(
        AudioParameters::AUDIO_FAKE, kChannelLayout,
        kSampleRate, kBitsPerSample, samples_per_packet);

    if (params_.IsValid()) {
      EXPECT_CALL(mock_event_handler_, OnCreated(NotNull()))
          .WillOnce(SignalEvent(&create_event_));
    }

    controller_ = AudioOutputController::Create(
        audio_manager_.get(), &mock_event_handler_, params_,
        &mock_sync_reader_);
    if (controller_)
      controller_->SetVolume(kTestVolume);

    EXPECT_EQ(params_.IsValid(), controller_ != NULL);
  }

  void Play() {
    // Expect the event handler to receive one OnPlaying() call.
    EXPECT_CALL(mock_event_handler_, OnPlaying(NotNull()))
        .WillOnce(SignalEvent(&play_event_));

    // During playback, the mock pretends to provide audio data rendered and
    // sent from the render process.
    EXPECT_CALL(mock_sync_reader_, UpdatePendingBytes(_))
        .Times(AtLeast(2));
    EXPECT_CALL(mock_sync_reader_, Read(_, _))
        .WillRepeatedly(DoAll(PopulateBuffer(),
                              SignalEvent(&read_event_),
                              Return(params_.frames_per_buffer())));
    EXPECT_CALL(mock_sync_reader_, DataReady())
        .WillRepeatedly(Return(true));

    controller_->Play();
  }

  void Pause() {
    // Expect the event handler to receive one OnPaused() call.
    EXPECT_CALL(mock_event_handler_, OnPaused(NotNull()))
        .WillOnce(SignalEvent(&pause_event_));

    controller_->Pause();
  }

  void ChangeDevice() {
    // Expect the event handler to receive one OnPaying() call and no OnPaused()
    // call.
    EXPECT_CALL(mock_event_handler_, OnPlaying(NotNull()))
        .WillOnce(SignalEvent(&play_event_));
    EXPECT_CALL(mock_event_handler_, OnPaused(NotNull()))
        .Times(0);

    // Simulate a device change event to AudioOutputController from the
    // AudioManager.
    audio_manager_->GetMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&AudioOutputController::OnDeviceChange, controller_));
  }

  void Divert(bool was_playing, bool will_be_playing) {
    if (was_playing) {
      // Expect the handler to receive one OnPlaying() call as a result of the
      // stream switching.
      EXPECT_CALL(mock_event_handler_, OnPlaying(NotNull()))
          .WillOnce(SignalEvent(&play_event_));
    }

    EXPECT_CALL(mock_stream_, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(mock_stream_, SetVolume(kTestVolume));
    if (will_be_playing) {
      EXPECT_CALL(mock_stream_, Start(NotNull()))
          .Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&mock_stream_, &MockAudioOutputStream::SetCallback));
    }
    // Always expect a Stop() call--even without a Start() call--since
    // AudioOutputController likes to play it safe and Stop() before any
    // Close().
    EXPECT_CALL(mock_stream_, Stop())
        .Times(AtLeast(1));

    controller_->StartDiverting(&mock_stream_);
  }

  void ReadDivertedAudioData() {
    scoped_ptr<AudioBus> dest = AudioBus::Create(params_);
    ASSERT_TRUE(!!mock_stream_.callback());
    const int frames_read =
        mock_stream_.callback()->OnMoreData(dest.get(), AudioBuffersState());
    EXPECT_LT(0, frames_read);
    EXPECT_EQ(kBufferNonZeroData, dest->channel(0)[0]);
  }

  void Revert(bool was_playing) {
    if (was_playing) {
      // Expect the handler to receive one OnPlaying() call as a result of the
      // stream switching back.
      EXPECT_CALL(mock_event_handler_, OnPlaying(NotNull()))
          .WillOnce(SignalEvent(&play_event_));
    }

    EXPECT_CALL(mock_stream_, Close());

    controller_->StopDiverting();
  }

  void Close() {
    EXPECT_CALL(mock_sync_reader_, Close());

    controller_->Close(MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  // These help make test sequences more readable.
  void DivertNeverPlaying() { Divert(false, false); }
  void DivertWillEventuallyBePlaying() { Divert(false, true); }
  void DivertWhilePlaying() { Divert(true, true); }
  void RevertWasNotPlaying() { Revert(false); }
  void RevertWhilePlaying() { Revert(true); }

  // These synchronize the main thread with key events taking place on other
  // threads.
  void WaitForCreate() { create_event_.Wait(); }
  void WaitForPlay() { play_event_.Wait(); }
  void WaitForReads() {
    // Note: Arbitrarily chosen, but more iterations causes tests to take
    // significantly more time.
    static const int kNumIterations = 3;
    for (int i = 0; i < kNumIterations; ++i) {
      read_event_.Wait();
    }
  }
  void WaitForPause() { pause_event_.Wait(); }

 private:
  MessageLoopForIO message_loop_;
  scoped_ptr<AudioManager> audio_manager_;
  MockAudioOutputControllerEventHandler mock_event_handler_;
  MockAudioOutputControllerSyncReader mock_sync_reader_;
  MockAudioOutputStream mock_stream_;
  base::WaitableEvent create_event_;
  base::WaitableEvent play_event_;
  base::WaitableEvent read_event_;
  base::WaitableEvent pause_event_;
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
  WaitForCreate();
  Play();
  WaitForPlay();
  WaitForReads();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayPauseClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  Play();
  WaitForPlay();
  WaitForReads();
  Pause();
  WaitForPause();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayPausePlayClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  Play();
  WaitForPlay();
  WaitForReads();
  Pause();
  WaitForPause();
  Play();
  WaitForPlay();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDeviceChangeClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  Play();
  WaitForPlay();
  WaitForReads();
  ChangeDevice();
  WaitForPlay();
  WaitForReads();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDivertRevertClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  Play();
  WaitForPlay();
  WaitForReads();
  DivertWhilePlaying();
  WaitForPlay();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  WaitForPlay();
  WaitForReads();
  Close();
}

TEST_F(AudioOutputControllerTest, PlayDivertRevertDivertRevertClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  Play();
  WaitForPlay();
  WaitForReads();
  DivertWhilePlaying();
  WaitForPlay();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  WaitForPlay();
  WaitForReads();
  DivertWhilePlaying();
  WaitForPlay();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  WaitForPlay();
  WaitForReads();
  Close();
}

TEST_F(AudioOutputControllerTest, DivertPlayPausePlayRevertClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  DivertWillEventuallyBePlaying();
  Play();
  WaitForPlay();
  ReadDivertedAudioData();
  Pause();
  WaitForPause();
  Play();
  WaitForPlay();
  ReadDivertedAudioData();
  RevertWhilePlaying();
  WaitForPlay();
  WaitForReads();
  Close();
}

TEST_F(AudioOutputControllerTest, DivertRevertClose) {
  Create(kSamplesPerPacket);
  WaitForCreate();
  DivertNeverPlaying();
  RevertWasNotPlaying();
  Close();
}

}  // namespace media
