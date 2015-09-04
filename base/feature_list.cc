// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace base {

namespace {

// Pointer to the FeatureList instance singleton that was set via
// FeatureList::SetInstance(). Does not use base/memory/singleton.h in order to
// have more control over initialization timing. Leaky.
FeatureList* g_instance = nullptr;

// Splits a comma-separated string containing feature names into a vector.
std::vector<std::string> SplitFeatureListString(const std::string& input) {
  return SplitString(input, ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
}

}  // namespace

FeatureList::FeatureList() : initialized_(false) {}

FeatureList::~FeatureList() {}

void FeatureList::InitializeFromCommandLine(
    const std::string& enable_features,
    const std::string& disable_features) {
  DCHECK(!initialized_);

  // Process disabled features first, so that disabled ones take precedence over
  // enabled ones (since RegisterOverride() uses insert()).
  for (const auto& feature_name : SplitFeatureListString(disable_features)) {
    RegisterOverride(feature_name, OVERRIDE_DISABLE_FEATURE);
  }
  for (const auto& feature_name : SplitFeatureListString(enable_features)) {
    RegisterOverride(feature_name, OVERRIDE_ENABLE_FEATURE);
  }
}

// static
bool FeatureList::IsEnabled(const Feature& feature) {
  return GetInstance()->IsFeatureEnabled(feature);
}

// static
FeatureList* FeatureList::GetInstance() {
  return g_instance;
}

// static
void FeatureList::SetInstance(scoped_ptr<FeatureList> instance) {
  DCHECK(!g_instance);
  instance->FinalizeInitialization();

  // Note: Intentional leak of global singleton.
  g_instance = instance.release();
}

// static
void FeatureList::ClearInstanceForTesting() {
  delete g_instance;
  g_instance = nullptr;
}

void FeatureList::FinalizeInitialization() {
  DCHECK(!initialized_);
  initialized_ = true;
}

bool FeatureList::IsFeatureEnabled(const Feature& feature) {
  DCHECK(initialized_);
  DCHECK(CheckFeatureIdentity(feature)) << feature.name;

  auto it = overrides_.find(feature.name);
  if (it != overrides_.end()) {
    const OverrideEntry& entry = it->second;
    // TODO(asvitkine) Expand this section as more support is added.
    return entry.overridden_state == OVERRIDE_ENABLE_FEATURE;
  }
  // Otherwise, return the default state.
  return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
}

void FeatureList::RegisterOverride(const std::string& feature_name,
                                   OverrideState overridden_state) {
  DCHECK(!initialized_);
  overrides_.insert(make_pair(feature_name, OverrideEntry(overridden_state)));
}

bool FeatureList::CheckFeatureIdentity(const Feature& feature) {
  AutoLock auto_lock(feature_identity_tracker_lock_);

  auto it = feature_identity_tracker_.find(feature.name);
  if (it == feature_identity_tracker_.end()) {
    // If it's not tracked yet, register it.
    feature_identity_tracker_[feature.name] = &feature;
    return true;
  }
  // Compare address of |feature| to the existing tracked entry.
  return it->second == &feature;
}

FeatureList::OverrideEntry::OverrideEntry(OverrideState overridden_state)
    : overridden_state(overridden_state) {}

}  // namespace base
