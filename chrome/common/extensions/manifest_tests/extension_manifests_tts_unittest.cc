// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, TtsEngine) {
  Testcase testcases[] = {
    Testcase("tts_engine_invalid_1.json",
             errors::kInvalidTts),
    Testcase("tts_engine_invalid_2.json",
             errors::kInvalidTtsVoices),
    Testcase("tts_engine_invalid_3.json",
             errors::kInvalidTtsVoices),
    Testcase("tts_engine_invalid_4.json",
             errors::kInvalidTtsVoicesVoiceName),
    Testcase("tts_engine_invalid_5.json",
             errors::kInvalidTtsVoicesLang),
    Testcase("tts_engine_invalid_6.json",
             errors::kInvalidTtsVoicesLang),
    Testcase("tts_engine_invalid_7.json",
             errors::kInvalidTtsVoicesGender),
    Testcase("tts_engine_invalid_8.json",
             errors::kInvalidTtsVoicesEventTypes),
    Testcase("tts_engine_invalid_9.json",
             errors::kInvalidTtsVoicesEventTypes)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("tts_engine_valid.json"));

  ASSERT_EQ(1u, extension->tts_voices().size());
  EXPECT_EQ("name", extension->tts_voices()[0].voice_name);
  EXPECT_EQ("en-US", extension->tts_voices()[0].lang);
  EXPECT_EQ("female", extension->tts_voices()[0].gender);
  EXPECT_EQ(3U, extension->tts_voices()[0].event_types.size());
}
