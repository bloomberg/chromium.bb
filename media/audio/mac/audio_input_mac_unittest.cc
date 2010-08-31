// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the mac audio input stream implementation.
// TODO(joth): See if we can generalize this for all platforms?

#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::Le;
using ::testing::NotNull;
using ::testing::StrictMock;

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD3(OnData, void(AudioInputStream* stream, const uint8* src,
                            uint32 size));
  MOCK_METHOD1(OnClose, void(AudioInputStream* stream));
  MOCK_METHOD2(OnError, void(AudioInputStream* stream, int code));
};

// Test fixture.
class AudioInputStreamMacTest : public testing::Test {
 protected:
  // From testing::Test.
  virtual void SetUp() {
    ias_ = NULL;
    AudioManager* audio_man = AudioManager::GetAudioManager();
    ASSERT_TRUE(NULL != audio_man);
    if (audio_man->HasAudioInputDevices()) {
      AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, 2,
                             kSampleRate, 16);
      ias_ = audio_man->MakeAudioInputStream(params, kSamplesPerCall);
    }
  }
  virtual void TearDown() {
    ias_->Close();
  }

  bool TestEnabled() const {
    return NULL != ias_;
  }

  static const int kSampleRate = 8000;
  static const int kSamplesPerCall = 3000;
  // This is the default callback implementation; it will assert the expected
  // calls were received when it is destroyed.
  StrictMock<MockAudioInputCallback> client_;
  // The audio input stream under test.
  AudioInputStream* ias_;
};

// Test that can it be created and closed.
TEST_F(AudioInputStreamMacTest, PCMInputStreamCreateAndClose) {
  if (!TestEnabled())
    return;
}

// Test that it can be opened and closed.
TEST_F(AudioInputStreamMacTest, PCMInputStreamOpenAndClose) {
  if (!TestEnabled())
    return;
  EXPECT_TRUE(ias_->Open());
}

// Test we handle a open then stop OK.
TEST_F(AudioInputStreamMacTest, PCMInputStreamOpenTheStop) {
  if (!TestEnabled())
    return;
  EXPECT_TRUE(ias_->Open());
  ias_->Stop();
}

// Record for a short time then stop. Make sure we get the callbacks.
TEST_F(AudioInputStreamMacTest, PCMInputStreamRecordCoupleSeconds) {
  if (!TestEnabled())
    return;
  ASSERT_TRUE(ias_->Open());
  // For a given sample rate, number of samples per callback, and recording time
  // we can estimate the number of callbacks we should get. We underestimate
  // this slightly, as the callback thread could be slow.
  static const double kRecordPeriodSecs = 1.5;
  static const int kMinExpectedCalls =
      (kRecordPeriodSecs * kSampleRate / kSamplesPerCall) - 1;
  // Check this is in reasonable bounds.
  EXPECT_GT(kMinExpectedCalls, 1);
  EXPECT_LT(kMinExpectedCalls, 10);

  static const uint kMaxBytesPerCall = 2 * 2 * kSamplesPerCall;
  EXPECT_CALL(client_, OnData(ias_, NotNull(),
                              AllOf(Gt(0u), Le(kMaxBytesPerCall))))
      .Times(AtLeast(kMinExpectedCalls));
  EXPECT_CALL(client_, OnClose(ias_));

  ias_->Start(&client_);
  usleep(kRecordPeriodSecs * base::Time::kMicrosecondsPerSecond);
  ias_->Stop();
}
