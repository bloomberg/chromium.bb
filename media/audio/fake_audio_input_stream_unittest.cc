// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/threading/platform_thread.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::NotNull;

namespace {

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockAudioInputCallback() {}

  MOCK_METHOD1(OnClose, void(AudioInputStream* stream));
  MOCK_METHOD2(OnError, void(AudioInputStream* stream, int error_code));
  MOCK_METHOD3(OnData, void(AudioInputStream* stream, const uint8* src,
                            uint32 size));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputCallback);
};

}

// ============================================================================
// Validate that the AudioManager::AUDIO_MOCK callbacks work.
// Crashes, http://crbug.com/49497.
TEST(FakeAudioInputTest, DISABLED_BasicCallbacks) {
  MockAudioInputCallback callback;
  EXPECT_CALL(callback, OnData(NotNull(), _, _)).Times(AtLeast(5));
  EXPECT_CALL(callback, OnError(NotNull(), _)).Times(Exactly(0));

  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  // Ask for one recorded packet every 50ms.
  AudioInputStream* stream = audio_man->MakeAudioInputStream(
      AudioParameters(AudioParameters::AUDIO_MOCK, 2, 8000, 8, 400));
  ASSERT_TRUE(NULL != stream);
  EXPECT_TRUE(stream->Open());
  stream->Start(&callback);

  // Give sufficient time to receive 5 / 6 packets.
  base::PlatformThread::Sleep(340);
  stream->Stop();
  stream->Close();
}
