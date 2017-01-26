// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/language_model.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::FloatEq;
using testing::Gt;
using testing::SizeIs;

namespace {

const char kLang1[] = "en";
const char kLang2[] = "de";
const char kLang3[] = "es";

}  // namespace

namespace translate {

bool operator==(const LanguageModel::LanguageInfo& lhs,
                const LanguageModel::LanguageInfo& rhs) {
  return lhs.language_code == rhs.language_code;
}

TEST(LanguageModelTest, ListSorted) {
  TestingPrefServiceSimple prefs;
  LanguageModel::RegisterProfilePrefs(prefs.registry());
  LanguageModel model(&prefs);

  for (int i = 0; i < 50; i++) {
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang2);
  }

  // Note: LanguageInfo's operator== only checks the language code, not the
  // frequency.
  EXPECT_THAT(model.GetTopLanguages(),
              ElementsAre(LanguageModel::LanguageInfo(kLang1, 0.0f),
                          LanguageModel::LanguageInfo(kLang2, 0.0f)));
}

TEST(LanguageModelTest, ListSortedReversed) {
  TestingPrefServiceSimple prefs;
  LanguageModel::RegisterProfilePrefs(prefs.registry());
  LanguageModel model(&prefs);

  for (int i = 0; i < 50; i++) {
    model.OnPageVisited(kLang2);
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang1);
  }

  // Note: LanguageInfo's operator== only checks the language code, not the
  // frequency.
  EXPECT_THAT(model.GetTopLanguages(),
              ElementsAre(LanguageModel::LanguageInfo(kLang1, 0.0f),
                          LanguageModel::LanguageInfo(kLang2, 0.0f)));
}

TEST(LanguageModelTest, RightFrequencies) {
  TestingPrefServiceSimple prefs;
  LanguageModel::RegisterProfilePrefs(prefs.registry());
  LanguageModel model(&prefs);

  for (int i = 0; i < 50; i++) {
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang1);
    model.OnPageVisited(kLang2);
  }

  // Corresponding frequencies are given by the model.
  EXPECT_THAT(model.GetLanguageFrequency(kLang1), FloatEq(0.75f));
  EXPECT_THAT(model.GetLanguageFrequency(kLang2), FloatEq(0.25f));
  // An unknown language gets frequency 0.
  EXPECT_THAT(model.GetLanguageFrequency(kLang3), 0);
}

TEST(LanguageModelTest, RareLanguageDiscarded) {
  TestingPrefServiceSimple prefs;
  LanguageModel::RegisterProfilePrefs(prefs.registry());
  LanguageModel model(&prefs);

  model.OnPageVisited(kLang2);

  for (int i = 0; i < 900; i++)
    model.OnPageVisited(kLang1);

  // Lang 2 is in the model.
  EXPECT_THAT(model.GetLanguageFrequency(kLang2), Gt(0.0f));

  // Another 100 visits cause the cleanup (total > 1000).
  for (int i = 0; i < 100; i++)
    model.OnPageVisited(kLang1);
  // Lang 2 is removed from the model.
  EXPECT_THAT(model.GetTopLanguages(),
              ElementsAre(LanguageModel::LanguageInfo{kLang1, 1}));
}

TEST(LanguageModelTest, ShouldClearHistoryIfAllTimes) {
  TestingPrefServiceSimple prefs;
  LanguageModel::RegisterProfilePrefs(prefs.registry());
  LanguageModel model(&prefs);

  for (int i = 0; i < 100; i++) {
    model.OnPageVisited(kLang1);
  }

  EXPECT_THAT(model.GetTopLanguages(), SizeIs(1));
  EXPECT_THAT(model.GetLanguageFrequency(kLang1), FloatEq(1.0));

  model.ClearHistory(base::Time(), base::Time::Max());

  EXPECT_THAT(model.GetTopLanguages(), SizeIs(0));
  EXPECT_THAT(model.GetLanguageFrequency(kLang1), FloatEq(0.0));
}

TEST(LanguageModelTest, ShouldNotClearHistoryIfNotAllTimes) {
  TestingPrefServiceSimple prefs;
  LanguageModel::RegisterProfilePrefs(prefs.registry());
  LanguageModel model(&prefs);

  for (int i = 0; i < 100; i++) {
    model.OnPageVisited(kLang1);
  }

  EXPECT_THAT(model.GetTopLanguages(), SizeIs(1));
  EXPECT_THAT(model.GetLanguageFrequency(kLang1), FloatEq(1.0));

  // Clearing only the last hour of the history has no effect.
  model.ClearHistory(base::Time::Now() - base::TimeDelta::FromHours(2),
                     base::Time::Max());

  EXPECT_THAT(model.GetTopLanguages(), SizeIs(1));
  EXPECT_THAT(model.GetLanguageFrequency(kLang1), FloatEq(1.0));
}

}  // namespace translate
