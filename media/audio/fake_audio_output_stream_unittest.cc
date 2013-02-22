// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kTestCallbacks = 5;

class FakeAudioOutputStreamTest : public testing::Test {
 public:
  FakeAudioOutputStreamTest()
      : audio_manager_(AudioManager::Create()),
        params_(
            AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_STEREO, 44100, 8, 128),
        source_(params_.channels(), 200.0, params_.sample_rate()),
        done_(false, false) {
    stream_ = audio_manager_->MakeAudioOutputStream(AudioParameters(params_));
    CHECK(stream_);

    time_between_callbacks_ = base::TimeDelta::FromMicroseconds(
        params_.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
        static_cast<float>(params_.sample_rate()));
  }

  virtual ~FakeAudioOutputStreamTest() {}

  void RunOnAudioThread() {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    ASSERT_TRUE(stream_->Open());
    stream_->Start(&source_);
  }

  void RunOnceOnAudioThread() {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    RunOnAudioThread();
    // Start() should immediately post a task to run the source callback, so we
    // should end up with only a single callback being run.
    audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
        &FakeAudioOutputStreamTest::EndTest, base::Unretained(this), 1));
  }

  void StopStartOnAudioThread() {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    stream_->Stop();
    stream_->Start(&source_);
  }

  void TimeCallbacksOnAudioThread(int callbacks) {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());

    if (source_.callbacks() == 0) {
      RunOnAudioThread();
      start_time_ = base::Time::Now();
    }

    // Keep going until we've seen the requested number of callbacks.
    if (source_.callbacks() < callbacks) {
      audio_manager_->GetMessageLoop()->PostDelayedTask(FROM_HERE, base::Bind(
          &FakeAudioOutputStreamTest::TimeCallbacksOnAudioThread,
          base::Unretained(this), callbacks), time_between_callbacks_ / 2);
    } else {
      end_time_ = base::Time::Now();
      EndTest(callbacks);
    }
  }

  void EndTest(int callbacks) {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    stream_->Stop();
    stream_->Close();
    EXPECT_LE(callbacks, source_.callbacks());
    EXPECT_EQ(0, source_.errors());
    done_.Signal();
  }

 protected:
  scoped_ptr<AudioManager> audio_manager_;
  AudioParameters params_;
  AudioOutputStream* stream_;
  SineWaveAudioSource source_;
  base::WaitableEvent done_;
  base::Time start_time_;
  base::Time end_time_;
  base::TimeDelta time_between_callbacks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputStreamTest);
};

// Ensure the fake audio stream runs on the audio thread and handles fires
// callbacks to the AudioSourceCallback.
TEST_F(FakeAudioOutputStreamTest, FakeStreamBasicCallback) {
  audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &FakeAudioOutputStreamTest::RunOnceOnAudioThread,
      base::Unretained(this)));
  done_.Wait();
}

// Ensure the time between callbacks is sane.
TEST_F(FakeAudioOutputStreamTest, TimeBetweenCallbacks) {
  audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &FakeAudioOutputStreamTest::TimeCallbacksOnAudioThread,
      base::Unretained(this), kTestCallbacks));

  done_.Wait();

  // There are only (kTestCallbacks - 1) intervals between kTestCallbacks.
  base::TimeDelta actual_time_between_callbacks =
      (end_time_ - start_time_) / (source_.callbacks() - 1);

  // Ensure callback time is no faster than the expected time between callbacks.
  EXPECT_TRUE(actual_time_between_callbacks >= time_between_callbacks_);

  // Softly check if the callback time is no slower than twice the expected time
  // between callbacks.  Since this test runs on the bots we can't be too strict
  // with the bounds.
  if (actual_time_between_callbacks > 2 * time_between_callbacks_)
    LOG(ERROR) << "Time between fake audio callbacks is too large!";
}

// Ensure Start()/Stop() on the stream doesn't generate too many callbacks.  See
// http://crbug.com/159049
TEST_F(FakeAudioOutputStreamTest, StartStopClearsCallbacks) {
  audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &FakeAudioOutputStreamTest::TimeCallbacksOnAudioThread,
      base::Unretained(this), kTestCallbacks));

  // Issue a Stop() / Start() in between expected callbacks to maximize the
  // chance of catching the FakeAudioOutputStream doing the wrong thing.
  audio_manager_->GetMessageLoop()->PostDelayedTask(FROM_HERE, base::Bind(
      &FakeAudioOutputStreamTest::StopStartOnAudioThread,
      base::Unretained(this)), time_between_callbacks_ / 2);

  // EndTest() will ensure the proper number of callbacks have occurred.
  done_.Wait();
}

}  // namespace media
