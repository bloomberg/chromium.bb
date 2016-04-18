// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class FakeAudioManagerFactory : public AudioManagerFactory {
 public:
  FakeAudioManagerFactory() {}
  ~FakeAudioManagerFactory() override {}

  ScopedAudioManagerPtr CreateInstance(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
      AudioLogFactory* audio_log_factory) override {
    ScopedAudioManagerPtr instance(
        new FakeAudioManager(std::move(task_runner),
                             std::move(worker_task_runner), audio_log_factory));
    // |created_instance_| is used for verifying. Ownership is transferred to
    // caller.
    created_instance_ = instance.get();
    return instance;
  }

  AudioManager* created_instance() { return created_instance_; }

 private:
  AudioManager* created_instance_;
};

}  // namespace

// Verifies that SetFactory has the intended effect.
TEST(AudioManagerFactoryTest, CreateInstance) {
  {
    base::TestMessageLoop message_loop;
    // Create an audio manager and verify that it is not null.
    ScopedAudioManagerPtr manager =
        AudioManager::CreateForTesting(base::ThreadTaskRunnerHandle::Get());
    ASSERT_NE(nullptr, manager.get());
  }

  // Set the factory. Note that ownership of |factory| is transferred to
  // AudioManager.
  FakeAudioManagerFactory* factory = new FakeAudioManagerFactory();
  AudioManager::SetFactory(factory);
  {
    base::TestMessageLoop message_loop;
    // Create the AudioManager instance. Verify that it matches the instance
    // provided by the factory.
    ScopedAudioManagerPtr manager =
        AudioManager::CreateForTesting(base::ThreadTaskRunnerHandle::Get());
    ASSERT_NE(nullptr, manager.get());
    ASSERT_EQ(factory->created_instance(), manager.get());
  }
  // Reset AudioManagerFactory to prevent factory from persisting to other
  // tests on the same process.
  AudioManager::ResetFactoryForTesting();
}

}  // namespace media
