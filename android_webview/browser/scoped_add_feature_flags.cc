// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/scoped_add_feature_flags.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"

namespace android_webview {

ScopedAddFeatureFlags::ScopedAddFeatureFlags(base::CommandLine* cl)
    : cl_(cl),
      enabled_features_(base::FeatureList::SplitFeatureListString(
          cl->GetSwitchValueASCII(switches::kEnableFeatures))),
      disabled_features_(base::FeatureList::SplitFeatureListString(
          cl->GetSwitchValueASCII(switches::kDisableFeatures))) {}

ScopedAddFeatureFlags::~ScopedAddFeatureFlags() {
  cl_->AppendSwitchASCII(switches::kEnableFeatures,
                         base::JoinString(enabled_features_, ","));
  cl_->AppendSwitchASCII(switches::kDisableFeatures,
                         base::JoinString(disabled_features_, ","));
}

void ScopedAddFeatureFlags::EnableIfNotSet(const base::Feature& feature) {
  AddFeatureIfNotSet(feature, true /* enable */);
}

void ScopedAddFeatureFlags::DisableIfNotSet(const base::Feature& feature) {
  AddFeatureIfNotSet(feature, false /* enable */);
}

void ScopedAddFeatureFlags::AddFeatureIfNotSet(const base::Feature& feature,
                                               bool enable) {
  const char* feature_name = feature.name;
  if (base::ContainsValue(enabled_features_, feature_name) ||
      base::ContainsValue(disabled_features_, feature_name)) {
    return;
  }
  if (enable) {
    enabled_features_.emplace_back(feature_name);
  } else {
    disabled_features_.emplace_back(feature_name);
  }
}

}  // namespace android_webview
