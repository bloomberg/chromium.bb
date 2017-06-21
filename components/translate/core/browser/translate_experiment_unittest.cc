// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_experiment.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/test/scoped_command_line.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

class TranslateExperimentTest : public testing::Test {
 public:
  void TearDown() override {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
  }
};

TEST_F(TranslateExperimentTest, TestOverrideUiLanguage) {
  // Experiment not enabled, so do nothing.
  std::string ui_lang;
  TranslateExperiment::OverrideUiLanguage("my", &ui_lang);
  EXPECT_TRUE(ui_lang.empty());

  // Enable the field trial.
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("TranslateUiLangTrial", "Enabled");

  // Test language overrides.
  TranslateExperiment::OverrideUiLanguage("my", &ui_lang);
  EXPECT_EQ(ui_lang, "ms");
  TranslateExperiment::OverrideUiLanguage("id", &ui_lang);
  EXPECT_EQ(ui_lang, "id");
  TranslateExperiment::OverrideUiLanguage("th", &ui_lang);
  EXPECT_EQ(ui_lang, "th");

  {
    // Force translate language command-line switch.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "force-translate-language", "es");
    TranslateExperiment::OverrideUiLanguage("th", &ui_lang);
    EXPECT_EQ(ui_lang, "es");
  }
}

TEST_F(TranslateExperimentTest, TestShouldOverrideBlocking) {
  // By default the experiments are disabled.
  EXPECT_FALSE(TranslateExperiment::ShouldOverrideBlocking("en", "en"));

  // Enable the field trial.
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("TranslateUiLangTrial", "Enabled");

  EXPECT_TRUE(TranslateExperiment::ShouldOverrideBlocking("en", "en"));
  EXPECT_FALSE(TranslateExperiment::ShouldOverrideBlocking("en", "es"));
}

}  // namespace

TEST_F(TranslateExperimentTest, TestInExperiment) {
  // By default the experiments are disabled.
  EXPECT_FALSE(TranslateExperiment::InExperiment());

  {
    // Experiment enabled using command-line switch.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "enable-translate-experiment");
    EXPECT_TRUE(TranslateExperiment::InExperiment());
  }

  {
    // Force translate language command-line switch.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "force-translate-language", "es");
    EXPECT_TRUE(TranslateExperiment::InExperiment());
  }

  // Enable the field trial.
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial("TranslateUiLangTrial", "Enabled");
  EXPECT_TRUE(TranslateExperiment::InExperiment());

  {
    // Experiment disabled using command-line switch.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitch(
        "disable-translate-experiment");
    EXPECT_FALSE(TranslateExperiment::InExperiment());
  }
}

}  // namespace translate
