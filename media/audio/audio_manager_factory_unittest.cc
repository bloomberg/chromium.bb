// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_factory.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class FakeAudioManagerFactory : public AudioManagerFactory {
 public:
  FakeAudioManagerFactory() {}
  ~FakeAudioManagerFactory() override {}

  AudioManager* CreateInstance(AudioLogFactory* audio_log_factory) override {
    // |created_instance_| is used for verifying. Ownership is transferred to
    // caller.
    created_instance_ = new FakeAudioManager(audio_log_factory);
    return created_instance_;
  }

  AudioManager* created_instance() { return created_instance_; }

 private:
  AudioManager* created_instance_;
};

}  // namespace

// Verifies that SetFactory has the intended effect.
TEST(AudioManagerFactoryTest, CreateInstance) {
  // Create an audio manager and verify that it is not null.
  scoped_ptr<AudioManager> manager(AudioManager::CreateForTesting());
  ASSERT_NE(nullptr, manager.get());
  manager.reset();

  // Set the factory. Note that ownership of |factory| is transferred to
  // AudioManager.
  FakeAudioManagerFactory* factory = new FakeAudioManagerFactory();
  AudioManager::SetFactory(factory);

  // Create the AudioManager instance. Verify that it matches the instance
  // provided by the factory.
  manager.reset(AudioManager::CreateForTesting());
  ASSERT_NE(nullptr, manager.get());
  ASSERT_EQ(factory->created_instance(), manager.get());

  // Reset AudioManagerFactory to prevent factory from persisting to other
  // tests on the same process. |manager| will reset when scope exits.
  AudioManager::ResetFactoryForTesting();
}

}  // namespace media
