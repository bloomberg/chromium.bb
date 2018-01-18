// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trial_creator.h"

#include <memory>

#include "android_webview/browser/aw_metrics_service_client.h"
#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"

namespace android_webview {
namespace {

// TODO(kmilka): Update to work properly in environments both with and without
// UMA enabled.
std::unique_ptr<const base::FieldTrial::EntropyProvider>
CreateLowEntropyProvider() {
  return std::unique_ptr<const base::FieldTrial::EntropyProvider>(
      // Since variations are only enabled for users opted in to UMA, it is
      // acceptable to use the SHA1EntropyProvider for randomization.
      new metrics::SHA1EntropyProvider(
          // Synchronous read of the client id is permitted as it is fast
          // enough to have minimal impact on startup time, and is behind the
          // webview-enable-finch flag.
          android_webview::AwMetricsServiceClient::GetClientId()));
}

// Synchronous read of variations data is permitted as it is fast
// enough to have minimal impact on startup time, and is behind the
// webview-enable-finch flag.
bool ReadVariationsSeedDataFromFile(PrefService* local_state) {
  // TODO(kmilka): The data read is being moved to java and the data will be
  // passed in via JNI.
  return false;
}

}  // anonymous namespace

AwFieldTrialCreator::AwFieldTrialCreator()
    : aw_field_trials_(std::make_unique<AwFieldTrials>()) {}

AwFieldTrialCreator::~AwFieldTrialCreator() {}

std::unique_ptr<PrefService> AwFieldTrialCreator::CreateLocalState() {
  scoped_refptr<PrefRegistrySimple> pref_registry =
      base::MakeRefCounted<PrefRegistrySimple>();

  // Register the variations prefs with default values that must be overridden.
  pref_registry->RegisterTimePref(variations::prefs::kVariationsSeedDate,
                                  base::Time());
  pref_registry->RegisterTimePref(variations::prefs::kVariationsLastFetchTime,
                                  base::Time());
  pref_registry->RegisterStringPref(variations::prefs::kVariationsCountry,
                                    std::string());
  pref_registry->RegisterStringPref(
      variations::prefs::kVariationsCompressedSeed, std::string());
  pref_registry->RegisterStringPref(variations::prefs::kVariationsSeedSignature,
                                    std::string());
  pref_registry->RegisterListPref(
      variations::prefs::kVariationsPermanentConsistencyCountry,
      std::make_unique<base::ListValue>());

  variations::SafeSeedManager::RegisterPrefs(pref_registry.get());

  pref_service_factory_.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());
  return pref_service_factory_.Create(pref_registry.get());
}

void AwFieldTrialCreator::SetUpFieldTrials() {
  AwMetricsServiceClient::LoadOrCreateClientId();

  DCHECK(!field_trial_list_);
  // Set the FieldTrialList singleton.
  field_trial_list_ =
      std::make_unique<base::FieldTrialList>(CreateLowEntropyProvider());

  std::unique_ptr<PrefService> local_state = CreateLocalState();

  if (!ReadVariationsSeedDataFromFile(local_state.get()))
    return;

  variations::UIStringOverrider ui_string_overrider;
  client_ = std::make_unique<AwVariationsServiceClient>();
  variations_field_trial_creator_ =
      std::make_unique<variations::VariationsFieldTrialCreator>(
          local_state.get(), client_.get(), ui_string_overrider);

  variations_field_trial_creator_->OverrideVariationsPlatform(
      variations::Study::PLATFORM_ANDROID_WEBVIEW);

  // Unused by WebView, but required by
  // VariationsFieldTrialCreator::SetupFieldTrials().
  // TODO(isherman): We might want a more genuine SafeSeedManager:
  // https://crbug.com/801771
  std::vector<std::string> variation_ids;
  std::set<std::string> unforceable_field_trials;
  variations::SafeSeedManager ignored_safe_seed_manager(true,
                                                        local_state.get());

  // Populates the FieldTrialList singleton via the static member functions.
  variations_field_trial_creator_->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, unforceable_field_trials,
      CreateLowEntropyProvider(), std::make_unique<base::FeatureList>(),
      &variation_ids, aw_field_trials_.get(), &ignored_safe_seed_manager);
}

}  // namespace android_webview
