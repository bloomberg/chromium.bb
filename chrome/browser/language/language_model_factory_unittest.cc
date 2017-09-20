// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/language_model_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/baseline_language_model.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::test::ScopedFeatureList;
using testing::Eq;
using testing::IsNull;
using testing::Not;

// Check that the language model is not returned unless the experiment is
// enabled.
TEST(LanguageModelFactoryTest, Disabled) {
  ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(language::kUseBaselineLanguageModel);
  content::TestBrowserThreadBundle thread_bundle;

  TestingProfile profile;
  const language::LanguageModel* const model =
      LanguageModelFactory::GetForBrowserContext(&profile);
  EXPECT_THAT(model, IsNull());

  Profile* const incognito = profile.GetOffTheRecordProfile();
  ASSERT_THAT(incognito, Not(IsNull()));
  EXPECT_THAT(LanguageModelFactory::GetForBrowserContext(incognito), IsNull());
}

// Check that Incognito language modeling is inherited from the user's profile.
TEST(LanguageModelFactoryTest, SharedWithIncognito) {
  ScopedFeatureList enable_feature;
  enable_feature.InitAndEnableFeature(language::kUseBaselineLanguageModel);
  content::TestBrowserThreadBundle thread_bundle;

  TestingProfile profile;
  const language::LanguageModel* const model =
      LanguageModelFactory::GetForBrowserContext(&profile);
  EXPECT_THAT(model, Not(IsNull()));

  Profile* const incognito = profile.GetOffTheRecordProfile();
  ASSERT_THAT(incognito, Not(IsNull()));
  EXPECT_THAT(LanguageModelFactory::GetForBrowserContext(incognito), Eq(model));
}
