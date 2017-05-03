// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"

namespace subresource_filter {
namespace testing {

// ScopedSubresourceFilterConfigurator ----------------------------------------

ScopedSubresourceFilterConfigurator::ScopedSubresourceFilterConfigurator(
    scoped_refptr<ConfigurationList> configs)
    : original_config_(GetAndSetActivateConfigurations(configs)) {}

ScopedSubresourceFilterConfigurator::ScopedSubresourceFilterConfigurator(
    Configuration config)
    : ScopedSubresourceFilterConfigurator(
          base::MakeShared<ConfigurationList>(std::move(config))) {}

ScopedSubresourceFilterConfigurator::~ScopedSubresourceFilterConfigurator() {
  GetAndSetActivateConfigurations(std::move(original_config_));
}

void ScopedSubresourceFilterConfigurator::ResetConfiguration(
    Configuration config) {
  GetAndSetActivateConfigurations(
      base::MakeShared<ConfigurationList>(std::move(config)));
}

// ScopedSubresourceFilterFeatureToggle ---------------------------------------

ScopedSubresourceFilterFeatureToggle::ScopedSubresourceFilterFeatureToggle(
    base::FeatureList::OverrideState feature_state) {
  if (feature_state == base::FeatureList::OVERRIDE_ENABLE_FEATURE)
    scoped_feature_list_.InitAndEnableFeature(kSafeBrowsingSubresourceFilter);
  else if (feature_state == base::FeatureList::OVERRIDE_DISABLE_FEATURE)
    scoped_feature_list_.InitAndDisableFeature(kSafeBrowsingSubresourceFilter);
}

ScopedSubresourceFilterFeatureToggle::~ScopedSubresourceFilterFeatureToggle() {}

}  // namespace testing
}  // namespace subresource_filter
