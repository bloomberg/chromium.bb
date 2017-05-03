// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "components/variations/variations_associated_data.h"

namespace subresource_filter {

namespace {

std::string TakeVariationParamOrReturnEmpty(
    std::map<std::string, std::string>* params,
    const std::string& key) {
  auto it = params->find(key);
  if (it == params->end())
    return std::string();
  std::string value = std::move(it->second);
  params->erase(it);
  return value;
}

ActivationLevel ParseActivationLevel(const base::StringPiece activation_level) {
  if (base::LowerCaseEqualsASCII(activation_level, kActivationLevelEnabled))
    return ActivationLevel::ENABLED;
  else if (base::LowerCaseEqualsASCII(activation_level, kActivationLevelDryRun))
    return ActivationLevel::DRYRUN;
  return ActivationLevel::DISABLED;
}

ActivationScope ParseActivationScope(const base::StringPiece activation_scope) {
  if (base::LowerCaseEqualsASCII(activation_scope, kActivationScopeAllSites))
    return ActivationScope::ALL_SITES;
  else if (base::LowerCaseEqualsASCII(activation_scope,
                                      kActivationScopeActivationList))
    return ActivationScope::ACTIVATION_LIST;
  return ActivationScope::NO_SITES;
}

ActivationList ParseActivationList(const base::StringPiece activation_lists) {
  ActivationList activation_list_type = ActivationList::NONE;
  for (const base::StringPiece& activation_list :
       base::SplitStringPiece(activation_lists, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (base::LowerCaseEqualsASCII(activation_list,
                                   kActivationListPhishingInterstitial)) {
      return ActivationList::PHISHING_INTERSTITIAL;
    } else if (base::LowerCaseEqualsASCII(
                   activation_list,
                   kActivationListSocialEngineeringAdsInterstitial)) {
      activation_list_type = ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
    } else if (base::LowerCaseEqualsASCII(activation_list,
                                          kActivationListSubresourceFilter)) {
      activation_list_type = ActivationList::SUBRESOURCE_FILTER;
    }
  }
  return activation_list_type;
}

double ParsePerformanceMeasurementRate(const std::string& rate) {
  double value = 0;
  if (!base::StringToDouble(rate, &value) || value < 0)
    return 0;
  return value < 1 ? value : 1;
}

bool ParseBool(const base::StringPiece value) {
  return base::LowerCaseEqualsASCII(value, "true");
}

Configuration ParseFieldTrialConfiguration() {
  Configuration configuration;

  std::map<std::string, std::string> params;
  base::GetFieldTrialParamsByFeature(kSafeBrowsingSubresourceFilter, &params);

  configuration.activation_level = ParseActivationLevel(
      TakeVariationParamOrReturnEmpty(&params, kActivationLevelParameterName));

  configuration.activation_scope = ParseActivationScope(
      TakeVariationParamOrReturnEmpty(&params, kActivationScopeParameterName));

  configuration.activation_list = ParseActivationList(
      TakeVariationParamOrReturnEmpty(&params, kActivationListsParameterName));

  configuration.performance_measurement_rate =
      ParsePerformanceMeasurementRate(TakeVariationParamOrReturnEmpty(
          &params, kPerformanceMeasurementRateParameterName));

  configuration.should_suppress_notifications =
      ParseBool(TakeVariationParamOrReturnEmpty(
          &params, kSuppressNotificationsParameterName));

  configuration.ruleset_flavor =
      TakeVariationParamOrReturnEmpty(&params, kRulesetFlavorParameterName);

  configuration.should_whitelist_site_on_reload =
      ParseBool(TakeVariationParamOrReturnEmpty(
          &params, kWhitelistSiteOnReloadParameterName));

  return configuration;
}

base::LazyInstance<base::Lock>::Leaky g_active_configurations_lock =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<scoped_refptr<ConfigurationList>>::Leaky
    g_active_configurations = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const base::Feature kSafeBrowsingSubresourceFilter{
    "SubresourceFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingSubresourceFilterExperimentalUI{
    "SubresourceFilterExperimentalUI", base::FEATURE_DISABLED_BY_DEFAULT};

// Legacy name `activation_state` is used in variation parameters.
const char kActivationLevelParameterName[] = "activation_state";
const char kActivationLevelDryRun[] = "dryrun";
const char kActivationLevelEnabled[] = "enabled";
const char kActivationLevelDisabled[] = "disabled";

const char kActivationScopeParameterName[] = "activation_scope";
const char kActivationScopeAllSites[] = "all_sites";
const char kActivationScopeActivationList[] = "activation_list";
const char kActivationScopeNoSites[] = "no_sites";

const char kActivationListsParameterName[] = "activation_lists";
const char kActivationListSocialEngineeringAdsInterstitial[] =
    "social_engineering_ads_interstitial";
const char kActivationListPhishingInterstitial[] = "phishing_interstitial";
const char kActivationListSubresourceFilter[] = "subresource_filter";

const char kRulesetFlavorParameterName[] = "ruleset_flavor";

const char kPerformanceMeasurementRateParameterName[] =
    "performance_measurement_rate";

const char kSuppressNotificationsParameterName[] = "suppress_notifications";

const char kWhitelistSiteOnReloadParameterName[] = "whitelist_site_on_reload";

Configuration::Configuration() = default;
Configuration::Configuration(ActivationLevel activation_level,
                             ActivationScope activation_scope,
                             ActivationList activation_list)
    : activation_level(activation_level),
      activation_scope(activation_scope),
      activation_list(activation_list) {}
Configuration::Configuration(Configuration&&) = default;
Configuration::~Configuration() = default;
Configuration& Configuration::operator=(Configuration&&) = default;

ConfigurationList::ConfigurationList(Configuration config)
    : config_(std::move(config)) {}
ConfigurationList::~ConfigurationList() = default;

scoped_refptr<ConfigurationList> GetActiveConfigurations() {
  base::AutoLock lock(g_active_configurations_lock.Get());
  if (!g_active_configurations.Get()) {
    g_active_configurations.Get() =
        base::MakeShared<ConfigurationList>(ParseFieldTrialConfiguration());
  }
  return g_active_configurations.Get();
}

namespace testing {

scoped_refptr<ConfigurationList> GetAndSetActivateConfigurations(
    scoped_refptr<ConfigurationList> new_configs) {
  base::AutoLock lock(g_active_configurations_lock.Get());
  auto old_configs = std::move(g_active_configurations.Get());
  g_active_configurations.Get() = std::move(new_configs);
  return old_configs;
}

}  // namespace testing

}  // namespace subresource_filter
