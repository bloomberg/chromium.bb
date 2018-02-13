// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_scope.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace subresource_filter {

// Encapsulates a set of parameters that define how the subresource filter
// feature should operate. Each configuration consists of three parts as
// described in detail below.
//
// There can be multiple configuration enabled at the same time. For each
// navigation, however, subresource filtering will be activated according to
// exactly one of these enabled configuration, if any. Namely, the configuration
// with the highest |priority| among those whose |activation_conditions| are
// otherwise satisfied for the navigation.
//
// Even when there are multiple enabled configurations, the RulesetService is
// currently only capable of fetching and indexing a single |ruleset_flavor|,
// which will be used for all navigations with subresource filtering activated,
// regardless of which configuration prescribed filtering for that navigation.
// This shared ruleset flavor will be the one lexicographically greatest.
//
// Experimenters wishing to use customized rulesets therefore must ensure that
// they set up the experimental state so that the ruleset chosen through this
// mechanism is compatible with all the enabled configurations (or disable some
// as needed).
struct Configuration {
  // The conditions that determine whether subresource filtering should be
  // activated for a given main frame navigation using this configuration.
  struct ActivationConditions {
    // The activation scope. That is, the subset of page loads where subresource
    // filtering should be activated according to this configuration. When set
    // to NO_SITES, this configuration will never be active.
    ActivationScope activation_scope = ActivationScope::NO_SITES;

    // The activation list to use when the |activation_scope| is
    // ACTIVATION_LIST, ignored otherwise.
    ActivationList activation_list = ActivationList::NONE;

    // The activation priority of this configuration. Used to break ties when
    // there are multiple configurations whose activation conditions are
    // otherwise satisfied. A greater value indicates higher priority.
    int priority = 0;

    // This boolean is set to true for a navigation which has forced activation,
    // despite other conditions not matching. It should never be possible to set
    // this via variation params.
    bool forced_activation = false;

    std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;
  };

  // The details of how subresource filtering should operate for a given main
  // frame navigation when it is activated using this configuration.
  struct ActivationOptions {
    // The maximum degree to which subresource filtering should be activated on
    // any RenderFrame. When set to DISABLED, this configuration will cause
    // subresource filtering to be de-activated for a navigation if this is the
    // highest priority configuration with its activation conditions met.
    ActivationLevel activation_level = ActivationLevel::DISABLED;

    // A number in the range [0, 1], indicating the fraction of page loads that
    // should have extended performance measurements enabled.
    double performance_measurement_rate = 0.0;

    // Whether notifications indicating that a subresource was disallowed should
    // be suppressed in the UI.
    bool should_suppress_notifications = false;

    // Whether to whitelist a site when a page loaded from that site is
    // reloaded.
    bool should_whitelist_site_on_reload = false;
  };

  // General settings that apply outside of the scope of a navigation.
  struct GeneralSettings {
    // The ruleset flavor to download through the component updater. The empty
    // string indicates that the default ruleset should be used.
    std::string ruleset_flavor;
  };

  // Do not forget updating operator==, operator<<, and any other necessary
  // methods when adding new fields here!

  Configuration();
  Configuration(ActivationLevel activation_level,
                ActivationScope activation_scope,
                ActivationList activation_list = ActivationList::NONE);
  Configuration(const Configuration&);
  Configuration(Configuration&&);
  ~Configuration();
  Configuration& operator=(const Configuration&);
  Configuration& operator=(Configuration&&);

  bool operator==(const Configuration& rhs) const;
  bool operator!=(const Configuration& rhs) const;

  std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;

  // Factory methods for preset configurations.
  //
  // To add a new preset:
  //  1.) Define a named factory method here.
  //  2.) Define a name for the configuration to be used in variation params.
  //  3.) Register it into |kAvailablePresetConfigurations| in the .cc file.
  //  4.) Update unittests to cover the new preset.
  static Configuration MakePresetForLiveRunOnPhishingSites();
  static Configuration MakePresetForPerformanceTestingDryRunOnAllSites();
  static Configuration MakePresetForLiveRunForBetterAds();

  // Not really a preset, but used as the configuration for forcing activation
  // (e.g. via devtools).
  static Configuration MakeForForcedActivation();

  ActivationConditions activation_conditions;
  ActivationOptions activation_options;
  GeneralSettings general_settings;
};

std::ostream& operator<<(std::ostream& os, const Configuration& config);

// Thread-safe, ref-counted wrapper around an immutable list of configurations.
class ConfigurationList : public base::RefCountedThreadSafe<ConfigurationList> {
 public:
  explicit ConfigurationList(std::vector<Configuration> configs);

  // Returns the lexicographically greatest flavor string that is prescribed by
  // any of the configurations. The caller must hold a reference to this
  // instance while using the returned string piece.
  base::StringPiece lexicographically_greatest_ruleset_flavor() const {
    return lexicographically_greatest_ruleset_flavor_;
  }

  // Retrieves the configurations pre-sorted in decreasing order of their
  // |activation_condition.priority|.
  const std::vector<Configuration>& configs_by_decreasing_priority() const {
    return configs_by_decreasing_priority_;
  }

 private:
  friend class base::RefCountedThreadSafe<ConfigurationList>;
  ~ConfigurationList();

  const std::vector<Configuration> configs_by_decreasing_priority_;
  const base::StringPiece lexicographically_greatest_ruleset_flavor_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationList);
};

// Retrieves all currently enabled subresource filtering configurations. The
// configurations are parsed on first access and then the result is cached.
//
// In tests, however, the config may be changed in-between navigations, so
// callers should not hold on to the result for long.
scoped_refptr<ConfigurationList> GetEnabledConfigurations();

bool HasEnabledConfiguration(const Configuration& config);

namespace testing {

// Returns the currently cached enabled ConfigurationList, if any, and replaces
// it with |new_configs|, which may be nullptr to clear the cache.
scoped_refptr<ConfigurationList> GetAndSetActivateConfigurations(
    scoped_refptr<ConfigurationList> new_configs);

}  // namespace testing

// Feature and variation parameter definitions -------------------------------

// The master toggle to enable/disable the Safe Browsing Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilter;

// Enables the new experimental UI for the Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilterExperimentalUI;

// Enables the tagging of ad frames and resource requests by using the
// subresource_filter component in dry-run mode.
extern const base::Feature kAdTagging;

// Name/values of the variation parameter controlling maximum activation level.
extern const char kActivationLevelParameterName[];
extern const char kActivationLevelDryRun[];
extern const char kActivationLevelEnabled[];
extern const char kActivationLevelDisabled[];

extern const char kActivationScopeParameterName[];
extern const char kActivationScopeAllSites[];
extern const char kActivationScopeActivationList[];
extern const char kActivationScopeNoSites[];

extern const char kActivationListsParameterName[];
extern const char kActivationListSocialEngineeringAdsInterstitial[];
extern const char kActivationListPhishingInterstitial[];
extern const char kActivationListSubresourceFilter[];
extern const char kActivationListBetterAds[];

extern const char kActivationPriorityParameterName[];

extern const char kPerformanceMeasurementRateParameterName[];

extern const char kSuppressNotificationsParameterName[];

extern const char kWhitelistSiteOnReloadParameterName[];

extern const char kRulesetFlavorParameterName[];

extern const char kEnablePresetsParameterName[];
extern const char kDisablePresetsParameterName[];
extern const char kPresetLiveRunOnPhishingSites[];
extern const char kPresetPerformanceTestingDryRunOnAllSites[];
extern const char kPresetLiveRunForBetterAds[];

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_H_
