// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_params_manager.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/variations_associated_data.h"

namespace variations {
namespace testing {

namespace {

// The fixed testing group created in the provided trail when setting up params.
const char kGroupTesting[] = "Testing";

base::FieldTrial* CreateFieldTrialWithParams(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values) {
  variations::AssociateVariationParams(trial_name, kGroupTesting, param_values);
  return base::FieldTrialList::CreateFieldTrial(trial_name, kGroupTesting);
}

}  // namespace

VariationParamsManager::VariationParamsManager()
    : field_trial_list_(new base::FieldTrialList(nullptr)),
      scoped_feature_list_(new base::test::ScopedFeatureList()) {}

VariationParamsManager::VariationParamsManager(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values)
    : VariationParamsManager() {
  SetVariationParams(trial_name, param_values);
}

VariationParamsManager::VariationParamsManager(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values,
    const std::set<std::string>& associated_features)
    : VariationParamsManager() {
  SetVariationParamsWithFeatureAssociations(trial_name, param_values,
                                            associated_features);
}

VariationParamsManager::~VariationParamsManager() {
  ClearAllVariationIDs();
  ClearAllVariationParams();
}

void VariationParamsManager::SetVariationParams(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values) {
  CreateFieldTrialWithParams(trial_name, param_values);
}

void VariationParamsManager::SetVariationParamsWithFeatureAssociations(
    const std::string& trial_name,
    const std::map<std::string, std::string>& param_values,
    const std::set<std::string>& associated_features) {
  base::FieldTrial* field_trial =
      CreateFieldTrialWithParams(trial_name, param_values);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  for (const std::string& feature_name : associated_features) {
    feature_list->RegisterFieldTrialOverride(
              feature_name,
              base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
              field_trial);
  }

  scoped_feature_list_->InitWithFeatureList(std::move(feature_list));
}

void VariationParamsManager::ClearAllVariationIDs() {
  variations::testing::ClearAllVariationIDs();
}

void VariationParamsManager::ClearAllVariationParams() {
  variations::testing::ClearAllVariationParams();
  // When the scoped feature list is destroyed, it puts back the original
  // feature list that was there when InitWithFeatureList() was called.
  scoped_feature_list_.reset(new base::test::ScopedFeatureList());
  // Ensure the destructor is called properly, so it can be freshly recreated.
  field_trial_list_.reset();
  field_trial_list_ = base::MakeUnique<base::FieldTrialList>(nullptr);
}

}  // namespace testing
}  // namespace variations
