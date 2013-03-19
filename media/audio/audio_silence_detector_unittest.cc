// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_silence_detector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::TestWithParam;
using ::testing::Values;

namespace media {

static const int kSampleRate = 48000;
static const int kFramesPerBuffer = 128;

namespace {

class MockObserver {
 public:
  MOCK_METHOD1(OnAudible, void(bool));
};

struct TestScenario {
  const float* data;
  int num_channels;
  int num_frames;
  float value_range;

  TestScenario(const float* d, int c, int f, float v)
      : data(d), num_channels(c), num_frames(f), value_range(v) {}
};

}  // namespace

class AudioSilenceDetectorTest : public TestWithParam<TestScenario> {
 public:
  AudioSilenceDetectorTest()
      : audio_manager_thread_(new base::Thread("AudioThread")),
        notification_received_(false, false) {
    audio_manager_thread_->Start();
    audio_message_loop_ = audio_manager_thread_->message_loop_proxy();
  }

  virtual ~AudioSilenceDetectorTest() {
    SyncWithAudioThread();
  }

  AudioSilenceDetector* silence_detector() const {
    return silence_detector_.get();
  }

  void StartSilenceDetector(float threshold, MockObserver* observer) {
    audio_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AudioSilenceDetectorTest::StartDetectorOnAudioThread,
                   base::Unretained(this), threshold, observer));
    SyncWithAudioThread();
  }

  void StopSilenceDetector() {
    audio_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AudioSilenceDetectorTest::StopDetectorOnAudioThread,
                   base::Unretained(this)));
    SyncWithAudioThread();
  }

  // Creates an AudioBus, sized and populated with kFramesPerBuffer frames of
  // data.  The given test |data| is repeated to fill the buffer.
  scoped_ptr<AudioBus> CreatePopulatedBuffer(
      const float* data, int num_channels, int num_frames) {
    scoped_ptr<AudioBus> bus = AudioBus::Create(num_channels, kFramesPerBuffer);
    for (int ch = 0; ch < num_channels; ++ch) {
      for (int frames = 0; frames < kFramesPerBuffer; frames += num_frames) {
        const int num_to_copy = std::min(num_frames, kFramesPerBuffer - frames);
        memcpy(bus->channel(ch) + frames, data + num_frames * ch,
               sizeof(float) * num_to_copy);
      }
    }
    return bus.Pass();
  }

  void SignalNotificationReceived() {
    notification_received_.Signal();
  }

  void WaitForNotificationReceived() {
    notification_received_.Wait();
  }

  // Post a task on the audio thread and block until it is executed.  This
  // provides a barrier: All previous tasks pending on the audio thread have
  // completed before this method returns.
  void SyncWithAudioThread() {
    base::WaitableEvent done(false, false);
    audio_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

 private:
  void StartDetectorOnAudioThread(float threshold, MockObserver* observer) {
    const AudioSilenceDetector::AudibleCallback notify_is_audible =
        base::Bind(&MockObserver::OnAudible, base::Unretained(observer));
    silence_detector_.reset(new AudioSilenceDetector(
        kSampleRate, base::TimeDelta::FromMilliseconds(1), threshold));
    silence_detector_->Start(notify_is_audible);
  }

  void StopDetectorOnAudioThread() {
    silence_detector_->Stop(true);
    silence_detector_.reset();
  }

  scoped_ptr<base::Thread> audio_manager_thread_;
  scoped_refptr<base::MessageLoopProxy> audio_message_loop_;

  base::WaitableEvent notification_received_;

  scoped_ptr<AudioSilenceDetector> silence_detector_;

  DISALLOW_COPY_AND_ASSIGN(AudioSilenceDetectorTest);
};

TEST_P(AudioSilenceDetectorTest, TriggersAtVariousThresholds) {
  static const float kThresholdsToTry[] =
      { 0.0f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f };

  const TestScenario& scenario = GetParam();

  for (size_t i = 0; i < arraysize(kThresholdsToTry); ++i) {
    MockObserver observer;

    // Start the detector.  This should cause a single callback to indicate
    // we're starting out in silence.
    EXPECT_CALL(observer, OnAudible(false))
        .WillOnce(InvokeWithoutArgs(
            this, &AudioSilenceDetectorTest::SignalNotificationReceived))
        .RetiresOnSaturation();
    StartSilenceDetector(kThresholdsToTry[i], &observer);
    WaitForNotificationReceived();

    // Send more data to the silence detector.  For some test scenarios, the
    // sound data will trigger a callback.
    const bool expect_a_callback = (kThresholdsToTry[i] < scenario.value_range);
    if (expect_a_callback) {
      EXPECT_CALL(observer, OnAudible(true))
          .WillOnce(InvokeWithoutArgs(
              this, &AudioSilenceDetectorTest::SignalNotificationReceived))
          .RetiresOnSaturation();
    }
    scoped_ptr<AudioBus> bus = CreatePopulatedBuffer(
        scenario.data, scenario.num_channels, scenario.num_frames);
    silence_detector()->Scan(bus.get(), bus->frames());
    if (expect_a_callback)
      WaitForNotificationReceived();

    // Stop the detector.  This should cause another callback to indicate we're
    // ending in silence.
    EXPECT_CALL(observer, OnAudible(false))
        .WillOnce(InvokeWithoutArgs(
            this, &AudioSilenceDetectorTest::SignalNotificationReceived))
        .RetiresOnSaturation();
    StopSilenceDetector();
    WaitForNotificationReceived();

    SyncWithAudioThread();
  }
}

static const float kAllZeros[] = {
  // left channel
  0.0f,
  // right channel
  0.0f
};

static const float kAllOnes[] = {
  // left channel
  1.0f,
  // right channel
  1.0f
};

static const float kSilentLeftChannel[] = {
  // left channel
  0.5f, 0.5f, 0.5f, 0.5f,
  // right channel
  0.0f, 0.25f, 0.0f, 0.0f
};

static const float kSilentRightChannel[] = {
  // left channel
  1.0f, 1.0f, 0.75f, 0.5f, 1.0f,
  // right channel
  0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

static const float kAtDifferentVolumesAndBias[] = {
  // left channel
  1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f,
  // right channel
  0.0f, 0.2f, 0.4f, 0.6f, 0.4f, 0.2f, 0.0f, 0.2f, 0.4f, 0.6f, 0.4f, 0.2f
};

INSTANTIATE_TEST_CASE_P(
    Scenarios, AudioSilenceDetectorTest,
    Values(
        TestScenario(kAllZeros, 2, 1, 0.0f),
        TestScenario(kAllOnes, 2, 1, 0.0f),
        TestScenario(kSilentLeftChannel, 2, 4, 0.25f),
        TestScenario(kSilentRightChannel, 2, 5, 0.5f),
        TestScenario(kAtDifferentVolumesAndBias, 2, 12, 0.6f)));

}  // namespace media
