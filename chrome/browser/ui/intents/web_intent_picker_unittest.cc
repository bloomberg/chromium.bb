// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "grit/theme_resources_standard.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebIntentPickerTest : public testing::Test {
 public:
  WebIntentPickerTest() {}
};

TEST_F(WebIntentPickerTest, GetStarImageIdsFromCWSRating) {
  struct TestCase {
    double rating;
    int index;
    int expected_star_image;
  } test_cases[] = {
    { 3.4, 0, IDR_CWS_STAR_FULL },
    { 3.4, 1, IDR_CWS_STAR_FULL },
    { 3.4, 2, IDR_CWS_STAR_FULL },
    { 3.4, 3, IDR_CWS_STAR_HALF },
    { 3.4, 4, IDR_CWS_STAR_EMPTY },
    { -1.0, 0, IDR_CWS_STAR_EMPTY },
    { -1.0, 1, IDR_CWS_STAR_EMPTY },
    { -1.0, 2, IDR_CWS_STAR_EMPTY },
    { -1.0, 3, IDR_CWS_STAR_EMPTY },
    { -1.0, 4, IDR_CWS_STAR_EMPTY },
    { 6.0, 0, IDR_CWS_STAR_FULL },
    { 6.0, 1, IDR_CWS_STAR_FULL },
    { 6.0, 2, IDR_CWS_STAR_FULL },
    { 6.0, 3, IDR_CWS_STAR_FULL },
    { 6.0, 4, IDR_CWS_STAR_FULL },
    { 1.6, 0, IDR_CWS_STAR_FULL },
    { 1.6, 1, IDR_CWS_STAR_HALF },
    { 1.6, 2, IDR_CWS_STAR_EMPTY },
    { 1.6, 3, IDR_CWS_STAR_EMPTY },
    { 1.6, 4, IDR_CWS_STAR_EMPTY },
    { 4.2, 0, IDR_CWS_STAR_FULL },
    { 4.2, 1, IDR_CWS_STAR_FULL },
    { 4.2, 2, IDR_CWS_STAR_FULL },
    { 4.2, 3, IDR_CWS_STAR_FULL },
    { 4.2, 4, IDR_CWS_STAR_EMPTY },
    { 3.7, 0, IDR_CWS_STAR_FULL },
    { 3.7, 1, IDR_CWS_STAR_FULL },
    { 3.7, 2, IDR_CWS_STAR_FULL },
    { 3.7, 3, IDR_CWS_STAR_FULL },
    { 3.7, 4, IDR_CWS_STAR_EMPTY },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    EXPECT_EQ(test_cases[i].expected_star_image,
              WebIntentPicker::GetNthStarImageIdFromCWSRating(
                  test_cases[i].rating,
                  test_cases[i].index));
  }
}
