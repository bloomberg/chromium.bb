// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/variations/variations_util.h"

#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/variations/fieldtrial_testing_config.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

class VariationsUtilTest : public ::testing::Test {
 public:
  VariationsUtilTest() : field_trial_list_(NULL) {}

  ~VariationsUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(VariationsUtilTest);
};

TEST_F(VariationsUtilTest, AssociateParamsFromString) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.B:a/%2F";
  ASSERT_TRUE(AssociateParamsFromString(kVariationsString));

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("/", variations::GetVariationParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), variations::GetVariationParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), variations::GetVariationParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(variations::GetVariationParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("/", params["a"]);
}

TEST_F(VariationsUtilTest, AssociateParamsFromStringWithSameStudy) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.A:a/x";
  ASSERT_FALSE(AssociateParamsFromString(kVariationsString));
}

TEST_F(VariationsUtilTest, AssociateParamsFromFieldTrialConfig) {
  const FieldTrialGroupParams array_kFieldTrialConfig_params[] = {{"x", "1"},
                                                                  {"y", "2"}};

  const FieldTrialTestingGroup array_kFieldTrialConfig_groups[] = {
      {"TestStudy1", "TestGroup1", array_kFieldTrialConfig_params, 2, NULL, 0,
       NULL, 0},
      {"TestStudy2", "TestGroup2", NULL, 0, NULL, 0, NULL, 0}};

  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_groups, 2,
  };

  base::FeatureList feature_list;
  AssociateParamsFromFieldTrialConfig(kConfig, &feature_list);

  EXPECT_EQ("1", variations::GetVariationParamValue("TestStudy1", "x"));
  EXPECT_EQ("2", variations::GetVariationParamValue("TestStudy1", "y"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(variations::GetVariationParams("TestStudy1", &params));
  EXPECT_EQ(2U, params.size());
  EXPECT_EQ("1", params["x"]);
  EXPECT_EQ("2", params["y"]);

  EXPECT_EQ("TestGroup1", base::FieldTrialList::FindFullName("TestStudy1"));
  EXPECT_EQ("TestGroup2", base::FieldTrialList::FindFullName("TestStudy2"));
}

TEST_F(VariationsUtilTest, AssociateFeaturesFromFieldTrialConfig) {
  const base::Feature kFeatureA{"A", base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kFeatureB{"B", base::FEATURE_ENABLED_BY_DEFAULT};
  const base::Feature kFeatureC{"C", base::FEATURE_DISABLED_BY_DEFAULT};
  const base::Feature kFeatureD{"D", base::FEATURE_ENABLED_BY_DEFAULT};

  const char* enable_features[] = {"A", "B"};
  const char* disable_features[] = {"C", "D"};

  const FieldTrialTestingGroup array_kFieldTrialConfig_groups[] = {
      {"TestStudy1", "TestGroup1", NULL, 0, enable_features, 2, NULL, 0},
      {"TestStudy2", "TestGroup2", NULL, 0, NULL, 0, disable_features, 2}};

  const FieldTrialTestingConfig kConfig = {
      array_kFieldTrialConfig_groups, 2,
  };

  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  AssociateParamsFromFieldTrialConfig(kConfig, feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));

  // Check the resulting feature and field trial states. Trials should not be
  // active until their associated features are queried.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("TestStudy1"));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureA));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureB));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("TestStudy1"));

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("TestStudy2"));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureC));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureD));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("TestStudy2"));
}

}  // namespace chrome_variations
