// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_session_impl.h"

#include "base/test/scoped_task_environment.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
const base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("file_path");
}

class AudioDebugRecordingSessionImplTest : public testing::Test {
 public:
  void CreateAudioManager() {
    mock_audio_manager_ =
        std::make_unique<MockAudioManager>(std::make_unique<TestAudioThread>());
    ASSERT_NE(nullptr, AudioManager::Get());
    ASSERT_EQ(static_cast<AudioManager*>(mock_audio_manager_.get()),
              AudioManager::Get());
  }

  void ShutdownAudioManager() { ASSERT_TRUE(mock_audio_manager_->Shutdown()); }

  void CreateDebugRecordingSession() {
    audio_debug_recording_session_impl_ =
        std::make_unique<media::AudioDebugRecordingSessionImpl>(file_path_);
  }

  void DestroyDebugRecordingSession() {
    audio_debug_recording_session_impl_.reset();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<MockAudioManager> mock_audio_manager_;
  std::unique_ptr<AudioDebugRecordingSessionImpl>
      audio_debug_recording_session_impl_;
  base::FilePath file_path_ = base::FilePath(kFilePath);
};

TEST_F(AudioDebugRecordingSessionImplTest,
       ConstructorEnablesAndDestructorDisablesDebugRecordingOnAudioManager) {
  ::testing::InSequence seq;

  CreateAudioManager();
  EXPECT_CALL(*mock_audio_manager_, EnableDebugRecording(file_path_));
  CreateDebugRecordingSession();

  EXPECT_CALL(*mock_audio_manager_, DisableDebugRecording());
  DestroyDebugRecordingSession();

  ShutdownAudioManager();
}

TEST_F(AudioDebugRecordingSessionImplTest,
       CreateDestroySessionDontCrashWithNoAudioManager) {
  ASSERT_EQ(nullptr, AudioManager::Get());
  CreateDebugRecordingSession();
  DestroyDebugRecordingSession();
}

}  // namespace media
