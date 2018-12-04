// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS platform implementation in Chrome OS.

#include "chrome/browser/speech/tts_chromeos.h"
#include "base/values.h"
#include "content/public/browser/tts_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

class TtsChromeosTest : public testing::Test {};

TEST_F(TtsChromeosTest, TestGetVoices) {
  TtsPlatformImplChromeOs* tts_chromeos =
      TtsPlatformImplChromeOs::GetInstance();
  std::unique_ptr<std::vector<content::VoiceData>> voices =
      std::make_unique<std::vector<content::VoiceData>>();
  tts_chromeos->GetVoices(voices.get());

  ASSERT_EQ(voices->size(), 1U);
}
