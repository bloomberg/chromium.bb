// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

const char kFieldTrialName[] = "EnhancedBookmarks";

ExternalComponentLoader::ExternalComponentLoader() {}

ExternalComponentLoader::~ExternalComponentLoader() {}

// This function will be removed after experiment goes to all users or go away.
bool ExternalComponentLoader::IsEnhancedBookmarksExperimentEnabled() {
  const char kFieldTrialDefaultGroupName[] = "Default";
  std::string field_trial_group_name =
      base::FieldTrialList::FindFullName(kFieldTrialName);
  if (field_trial_group_name.empty() ||
      field_trial_group_name == kFieldTrialDefaultGroupName) {
    return false;
  }
  std::string app_id =
      chrome_variations::GetVariationParamValue(kFieldTrialName, "id");
  FeatureProvider* feature_provider = FeatureProvider::GetPermissionFeatures();
  Feature* feature = feature_provider->GetFeature("metricsPrivate");
  return (feature && feature->IsIdInWhitelist(app_id));
}

void ExternalComponentLoader::StartLoading() {
  prefs_.reset(new base::DictionaryValue());
  std::string appId = extension_misc::kInAppPaymentsSupportAppId;
  prefs_->SetString(appId + ".external_update_url",
                    extension_urls::GetWebstoreUpdateUrl().spec());

  if (IsEnhancedBookmarksExperimentEnabled() &&
      (CommandLine::ForCurrentProcess()->
          GetSwitchValueASCII(switches::kEnableEnhancedBookmarks) != "0")) {
    std::string app_id =
        chrome_variations::GetVariationParamValue(kFieldTrialName, "id");
    prefs_->SetString(app_id + ".external_update_url",
                      extension_urls::GetWebstoreUpdateUrl().spec());
  }
  LoadFinished();
}

}  // namespace extensions
