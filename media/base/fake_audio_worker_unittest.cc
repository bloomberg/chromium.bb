// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_worker.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kTestCallbacks = 5;

class FakeAudioWorkerTest : public testing::Test {
 public:
  FakeAudioWorkerTest()
      : params_(AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_STEREO, 44100, 128),
        fake_worker_(scoped_task_environment_.GetMainThreadTaskRunner(),
                     params_) {
    time_between_callbacks_ = base::TimeDelta::FromMicroseconds(
        params_.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
        static_cast<float>(params_.sample_rate()));
  }

  ~FakeAudioWorkerTest() override = default;

  void CalledByFakeWorker(base::TimeTicks ideal_time, base::TimeTicks now) {
    callbacks_.push_back(base::TimeTicks::Now());
  }

  void RunOnAudioThread() {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    fake_worker_.Start(base::BindRepeating(
        &FakeAudioWorkerTest::CalledByFakeWorker, base::Unretained(this)));
  }

  void RunOnceOnAudioThread() {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    RunOnAudioThread();
    // Start() should immediately post a task to run the callback, so we
    // should end up with only a single callback being run.
    TaskRunner()->PostTask(FROM_HERE,
                           base::BindOnce(&FakeAudioWorkerTest::EndTest,
                                          base::Unretained(this), 1));
  }

  void StopStartOnAudioThread() {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    fake_worker_.Stop();
    RunOnAudioThread();
  }

  void TimeCallbacksOnAudioThread(size_t callbacks) {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());

    if (callbacks_.size() == 0) {
      RunOnAudioThread();
    }

    // Keep going until we've seen the requested number of callbacks.
    if (callbacks_.size() < callbacks) {
      TaskRunner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                         base::Unretained(this), callbacks),
          time_between_callbacks_ / 2);
    } else {
      EndTest(callbacks);
    }
  }

  void EndTest(size_t callbacks) {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    fake_worker_.Stop();
    EXPECT_LE(callbacks, callbacks_.size());
  }

  scoped_refptr<base::SingleThreadTaskRunner> TaskRunner() {
    return scoped_task_environment_.GetMainThreadTaskRunner();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      base::test::ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};
  AudioParameters params_;
  FakeAudioWorker fake_worker_;
  base::TimeDelta time_between_callbacks_;
  std::vector<base::TimeTicks> callbacks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioWorkerTest);
};

// Ensure the worker runs on the audio thread and fires callbacks.
// TODO(https://crbug.com/945486): Flakily failing on Fuchsia.
#if defined(OS_FUCHSIA)
#define MAYBE_FakeBasicCallback DISABLED_FakeBasicCallback
#else
#define MAYBE_FakeBasicCallback FakeBasicCallback
#endif
TEST_F(FakeAudioWorkerTest, MAYBE_FakeBasicCallback) {
  scoped_task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeAudioWorkerTest::RunOnceOnAudioThread,
                                base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
}

// Ensure the time between callbacks is correct.
TEST_F(FakeAudioWorkerTest, TimeBetweenCallbacks) {
  TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                     base::Unretained(this), kTestCallbacks));
  scoped_task_environment_.FastForwardUntilNoTasksRemain();

  // There are only (kTestCallbacks - 1) intervals between kTestCallbacks.
  base::TimeTicks start_time = callbacks_.front();
  std::vector<base::TimeTicks> expected_callback_times;
  for (size_t i = 0; i < callbacks_.size(); i++) {
    base::TimeTicks expected = start_time + time_between_callbacks_ * i;
    expected_callback_times.push_back(expected);
  }

  std::vector<int64_t> time_between_callbacks;
  for (size_t i = 0; i < callbacks_.size() - 1; i++) {
    time_between_callbacks.push_back(
        (callbacks_.at(i + 1) - callbacks_.at(i)).InMilliseconds());
  }

  EXPECT_THAT(time_between_callbacks,
              testing::Each(time_between_callbacks_.InMilliseconds()));
}

// Ensure Start()/Stop() on the worker doesn't generate too many callbacks. See
// http://crbug.com/159049.
// Flaky test; see crbug.com/974078
TEST_F(FakeAudioWorkerTest, DISABLED_StartStopClearsCallbacks) {
  TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                     base::Unretained(this), kTestCallbacks));

  // Issue a Stop() / Start() in between expected callbacks to maximize the
  // chance of catching the worker doing the wrong thing.
  scoped_task_environment_.FastForwardBy(time_between_callbacks_ / 2);
  TaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeAudioWorkerTest::StopStartOnAudioThread,
                                base::Unretained(this)));

  // EndTest() will ensure the proper number of callbacks have occurred.
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
}
}  // namespace media
