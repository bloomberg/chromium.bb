// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_thread_hang_monitor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Test;

namespace media {

namespace {

constexpr int kStarted =
    static_cast<int>(AudioThreadHangMonitor::ThreadStatus::kStarted);
constexpr int kHung =
    static_cast<int>(AudioThreadHangMonitor::ThreadStatus::kHung);
constexpr int kRecovered =
    static_cast<int>(AudioThreadHangMonitor::ThreadStatus::kRecovered);

}  // namespace

class AudioThreadHangMonitorTest : public Test {
 public:
  AudioThreadHangMonitorTest()
      : task_env_(base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        histograms_(),
        audio_thread_("Audio thread"),
        hang_monitor_({nullptr, base::OnTaskRunnerDeleter(nullptr)}) {
    CHECK(audio_thread_.Start());
    // We must inject the main thread task runner as the hang monitor task
    // runner since ScopedTaskEnvironment::FastForwardBy only works for the main
    // thread.
    hang_monitor_ = AudioThreadHangMonitor::Create(
        false, task_env_.GetMockTickClock(), audio_thread_.task_runner(),
        task_env_.GetMainThreadTaskRunner());
  }

  ~AudioThreadHangMonitorTest() override {
    hang_monitor_.reset();
    task_env_.RunUntilIdle();
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  void FlushAudioThread() {
    base::WaitableEvent ev;
    audio_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&ev)));
    ev.Wait();
  }

  void BlockAudioThreadUntilEvent() {
    // We keep |event_| as a member of the test fixture to make sure that the
    // audio thread terminates before |event_| is destructed.
    event_.Reset();
    audio_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Wait, base::Unretained(&event_)));
  }

  base::WaitableEvent event_;
  base::test::ScopedTaskEnvironment task_env_;
  base::HistogramTester histograms_;
  base::Thread audio_thread_;
  AudioThreadHangMonitor::Ptr hang_monitor_;
};

TEST_F(AudioThreadHangMonitorTest, LogsThreadStarted) {
  RunUntilIdle();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));
}

TEST_F(AudioThreadHangMonitorTest, DoesNotLogThreadHungWhenOk) {
  RunUntilIdle();

  for (int i = 0; i < 10; ++i) {
    // Flush the audio thread, then advance the clock. The audio thread should
    // register as "alive" every time.
    FlushAudioThread();
    task_env_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));
}

TEST_F(AudioThreadHangMonitorTest, LogsHungWhenAudioThreadIsBlocked) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       LogsRecoveredWhenAudioThreadIsBlockedThenRecovers) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  event_.Signal();

  for (int i = 0; i < 10; ++i) {
    // Flush the audio thread, then advance the clock. The audio thread should
    // register as "alive" every time.
    FlushAudioThread();
    task_env_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1),
                          base::Bucket(kRecovered, 1)));
}

}  // namespace media
