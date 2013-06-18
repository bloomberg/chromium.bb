// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_audio_controller.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "media/audio/audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Sample audio data
const char kTestAudioData[] = "RIFF\x26\x00\x00\x00WAVEfmt \x10\x00\x00\x00"
    "\x01\x00\x02\x00\x80\xbb\x00\x00\x00\x77\x01\x00\x02\x00\x10\x00"
    "data\x04\x00\x00\x00\x01\x00\x01\x00";
const char kInvalidAudioData[] = "invalid audio data";
const char kDummyNotificationId[] = "dummy_id";
const char kDummyNotificationId2[] = "dummy_id2";

void CopyResultAndQuit(
    base::RunLoop* run_loop, size_t *result_ptr, size_t result) {
  *result_ptr = result;
  run_loop->Quit();
}

} // namespace

class NotificationAudioControllerTest : public testing::Test {
 public:
  NotificationAudioControllerTest() {
    audio_manager_.reset(media::AudioManager::Create());
    notification_audio_controller_ = new NotificationAudioController();
    notification_audio_controller_->UseFakeAudioOutputForTest();
  }

  virtual ~NotificationAudioControllerTest() {
    notification_audio_controller_->RequestShutdown();
    base::RunLoop().RunUntilIdle();
    audio_manager_.reset();
  }

 protected:
  NotificationAudioController* notification_audio_controller() {
    return notification_audio_controller_;
  }
  Profile* profile() { return &profile_; }

  size_t GetHandlersSize() {
    base::RunLoop run_loop;
    size_t result = 0;
    notification_audio_controller_->GetAudioHandlersSizeForTest(
        base::Bind(&CopyResultAndQuit, &run_loop, &result));
    run_loop.Run();
    return result;
  }

 private:
  base::MessageLoopForUI message_loop_;
  TestingProfile profile_;
  NotificationAudioController* notification_audio_controller_;
  scoped_ptr<media::AudioManager> audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(NotificationAudioControllerTest);
};

TEST_F(NotificationAudioControllerTest, PlaySound) {
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      profile(),
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  // Still playing the sound, not empty yet.
  EXPECT_EQ(1u, GetHandlersSize());
}

// TODO(mukai): http://crbug.com/246061
#if !defined(OS_WIN) && !defined(ARCH_CPU_X86_64)
TEST_F(NotificationAudioControllerTest, PlayInvalidSound) {
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      profile(),
      base::StringPiece(kInvalidAudioData, arraysize(kInvalidAudioData)));
  EXPECT_EQ(0u, GetHandlersSize());
}
#endif

TEST_F(NotificationAudioControllerTest, PlayTwoSounds) {
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      profile(),
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId2,
      profile(),
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  EXPECT_EQ(2u, GetHandlersSize());
}

TEST_F(NotificationAudioControllerTest, PlaySoundTwice) {
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      profile(),
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      profile(),
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  EXPECT_EQ(1u, GetHandlersSize());
}

#if defined(THREAD_SANITIZER)
// This test fails under ThreadSanitizer v2, see http://crbug.com/247440.
#define MAYBE_MultiProfiles DISABLED_MultiProfiles
#else
#define MAYBE_MultiProfiles MultiProfiles
#endif
TEST_F(NotificationAudioControllerTest, MAYBE_MultiProfiles) {
  TestingProfile profile2;

  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      profile(),
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  notification_audio_controller()->RequestPlayNotificationSound(
      kDummyNotificationId,
      &profile2,
      base::StringPiece(kTestAudioData, arraysize(kTestAudioData)));
  EXPECT_EQ(2u, GetHandlersSize());
}
