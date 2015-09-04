// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/renderer_host/media/audio_output_device_enumerator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

namespace {
class MockAudioManager : public media::FakeAudioManager {
 public:
  MockAudioManager() : FakeAudioManager(&fake_audio_log_factory_) {}
  ~MockAudioManager() override {}
  MOCK_METHOD1(GetAudioOutputDeviceNames, void(media::AudioDeviceNames*));

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};
}  // namespace

class AudioOutputDeviceEnumeratorTest : public ::testing::Test {
 public:
  AudioOutputDeviceEnumeratorTest()
      : thread_bundle_(), task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    audio_manager_.reset(new MockAudioManager());
  }

  ~AudioOutputDeviceEnumeratorTest() override {}

  MOCK_METHOD1(MockCallback, void(const AudioOutputDeviceEnumeration&));

  // Performs n calls to MockCallback (one by QuitCallback)
  // and n-1 calls to Enumerate
  void EnumerateCallback(AudioOutputDeviceEnumerator* enumerator,
                         int n,
                         const AudioOutputDeviceEnumeration& result) {
    MockCallback(result);
    if (n > 1) {
      enumerator->Enumerate(
          base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCallback,
                     base::Unretained(this), enumerator, n - 1));
    } else {
      enumerator->Enumerate(
          base::Bind(&AudioOutputDeviceEnumeratorTest::QuitCallback,
                     base::Unretained(this)));
    }
  }

  void QuitCallback(const AudioOutputDeviceEnumeration& result) {
    MockCallback(result);
    task_runner_->PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

 protected:
  scoped_ptr<MockAudioManager> audio_manager_;
  TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RunLoop run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceEnumeratorTest);
};

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateWithCache) {
  const int num_calls = 10;
  EXPECT_CALL(*audio_manager_, GetAudioOutputDeviceNames(_)).Times(1);
  EXPECT_CALL(*this, MockCallback(_)).Times(num_calls);
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_MANUAL_INVALIDATION);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCallback,
                 base::Unretained(this), &enumerator, num_calls - 1));
  run_loop_.Run();
}

TEST_F(AudioOutputDeviceEnumeratorTest, EnumerateWithNoCache) {
  const int num_calls = 10;
  EXPECT_CALL(*audio_manager_, GetAudioOutputDeviceNames(_)).Times(num_calls);
  EXPECT_CALL(*this, MockCallback(_)).Times(num_calls);
  AudioOutputDeviceEnumerator enumerator(
      audio_manager_.get(),
      AudioOutputDeviceEnumerator::CACHE_POLICY_NO_CACHING);
  enumerator.Enumerate(
      base::Bind(&AudioOutputDeviceEnumeratorTest::EnumerateCallback,
                 base::Unretained(this), &enumerator, num_calls - 1));
  run_loop_.Run();
}

}  // namespace content
