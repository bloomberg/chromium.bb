// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/spellcheck_common.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using chrome::spellcheck_common::IsMultilingualSpellcheckEnabled;
using chrome::spellcheck_common::kMultilingualSpellcheckFieldTrial;

TEST(SpellcheckCommonTest, MultilingualByDefault) {
  EXPECT_TRUE(IsMultilingualSpellcheckEnabled());
}

TEST(SpellcheckCommonTest, CanDisableMultlingualInFieldTrial) {
  base::FieldTrialList trials(new metrics::SHA1EntropyProvider("foo"));
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(kMultilingualSpellcheckFieldTrial,
                                             "Disabled");
  trial->group();

  EXPECT_FALSE(IsMultilingualSpellcheckEnabled());
}

TEST(SpellcheckCommonTest, CanDisableMultlingualFromCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableMultilingualSpellChecker);

  EXPECT_FALSE(IsMultilingualSpellcheckEnabled());
}

TEST(SpellcheckCommonTest, CanForceMultlingualFromCommandLine) {
  base::FieldTrialList trials(new metrics::SHA1EntropyProvider("foo"));
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(kMultilingualSpellcheckFieldTrial,
                                             "Disabled");
  trial->group();
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableMultilingualSpellChecker);

  EXPECT_TRUE(IsMultilingualSpellcheckEnabled());
}

}  // namespace
