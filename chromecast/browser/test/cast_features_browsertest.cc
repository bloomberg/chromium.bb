// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_features.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/test/cast_browser_test.h"
#include "components/prefs/pref_service.h"

namespace chromecast {
namespace shell {
namespace {

const base::Feature kEnableFoo("enable_foo", base::FEATURE_DISABLED_BY_DEFAULT);
const base::Feature kEnableBar("enable_bar", base::FEATURE_ENABLED_BY_DEFAULT);
const base::Feature kEnableBaz("enable_baz", base::FEATURE_DISABLED_BY_DEFAULT);
const base::Feature kEnableBat("enable_bat", base::FEATURE_ENABLED_BY_DEFAULT);

const base::Feature kEnableParams("enable_params",
                                  base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

class CastFeaturesBrowserTest : public CastBrowserTest {
 public:
  CastFeaturesBrowserTest() {}
  ~CastFeaturesBrowserTest() override {}

  // Write |dcs_features| to the pref store. This method is intended to be
  // overridden in internal test to utilize the real production codepath for
  // setting features from the server.
  virtual void SetFeatures(const base::DictionaryValue& dcs_features) {
    auto pref_features = GetOverriddenFeaturesForStorage(dcs_features);
    CastBrowserProcess::GetInstance()->pref_service()->Set(
        prefs::kLatestDCSFeatures, pref_features);
  }

  static void ClearFeaturesInPrefs() {
    DCHECK(CastBrowserProcess::GetInstance());
    DCHECK(CastBrowserProcess::GetInstance()->pref_service());
    CastBrowserProcess::GetInstance()->pref_service()->Set(
        prefs::kLatestDCSFeatures, base::DictionaryValue());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CastFeaturesBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest, EmptyTest) {
  // Default values should be returned.
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableFoo));
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBar));
}

// Test that set features activate on the next boot. Part 1 of 2.
IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest,
                       PRE_TestFeaturesActivateOnBoot) {
  // Default values should be returned.
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableFoo));
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBar));
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableBaz));
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBat));

  // Set the features to be used on next boot.
  base::DictionaryValue features;
  features.SetBoolean("enable_foo", true);
  features.SetBoolean("enable_bat", false);
  SetFeatures(features);

  // Default values should still be returned until next boot.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kEnableFoo));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kEnableBar));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kEnableBaz));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kEnableBat));
}

// Test that features activate on the next boot. Part 2 of 2.
IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest, TestFeaturesActivateOnBoot) {
  // Overriden values set in test case above should be set.
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableFoo));
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBar));
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableBaz));
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableBat));

  // Clear the features for other tests.
  ClearFeaturesInPrefs();
}

// Test that features with params activate on boot. Part 1 of 2.
IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest, PRE_TestParamsActivateOnBoot) {
  // Default value should be returned.
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableParams));

  // Set the features to be used on next boot.
  base::DictionaryValue features;
  auto params = base::MakeUnique<base::DictionaryValue>();
  params->SetBoolean("bool_param", true);
  params->SetBoolean("bool_param_2", false);
  params->SetString("str_param", "foo");
  params->SetDouble("doub_param", 3.14159);
  params->SetInteger("int_param", 76543);
  features.Set("enable_params", std::move(params));
  SetFeatures(features);

  // Default value should still be returned until next boot.
  EXPECT_FALSE(base::FeatureList::IsEnabled(kEnableParams));
}

// Test that features with params activate on boot. Part 2 of 2.
IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest, TestParamsActivateOnBoot) {
  // Check that the feature is now enabled.
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableParams));

  // Check that the params are populated and correct.
  ASSERT_TRUE(base::GetFieldTrialParamByFeatureAsBool(
      kEnableParams, "bool_param", false /* default_value */));
  ASSERT_FALSE(base::GetFieldTrialParamByFeatureAsBool(
      kEnableParams, "bool_param_2", true /* default_value */));
  ASSERT_EQ("foo",
            base::GetFieldTrialParamValueByFeature(kEnableParams, "str_param"));
  ASSERT_EQ(76543, base::GetFieldTrialParamByFeatureAsInt(
                       kEnableParams, "int_param", 0 /* default_value */));
  ASSERT_EQ(3.14159, base::GetFieldTrialParamByFeatureAsDouble(
                         kEnableParams, "doub_param", 0.0 /* default_value */));

  // Check that no extra parameters are set.
  std::map<std::string, std::string> params_out;
  ASSERT_TRUE(base::GetFieldTrialParamsByFeature(kEnableParams, &params_out));
  ASSERT_EQ(5u, params_out.size());

  // Clear the features for other tests.
  ClearFeaturesInPrefs();
}

// Test that only well-formed features are persisted to disk. Part 1 of 2.
IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest,
                       PRE_TestOnlyWellFormedFeaturesPersisted) {
  // Default values should be returned.
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableFoo));
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBar));
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableBaz));
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBat));

  // Set both good parameters...
  base::DictionaryValue features;
  features.SetBoolean("enable_foo", true);
  features.SetBoolean("enable_bat", false);

  // ... and bad parameters.
  features.SetString("enable_bar", "False");
  features.Set("enable_baz", base::MakeUnique<base::ListValue>());

  SetFeatures(features);
}

// Test that only well-formed features are persisted to disk. Part 2 of 2.
IN_PROC_BROWSER_TEST_F(CastFeaturesBrowserTest,
                       TestOnlyWellFormedFeaturesPersisted) {
  // Only the well-formed parameters should be overriden.
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableFoo));
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableBat));

  // The other should take default values.
  ASSERT_TRUE(base::FeatureList::IsEnabled(kEnableBar));
  ASSERT_FALSE(base::FeatureList::IsEnabled(kEnableBaz));
}

}  // namespace shell
}  // namespace chromecast
