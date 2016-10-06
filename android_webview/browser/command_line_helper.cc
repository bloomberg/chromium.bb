// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/command_line_helper.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"

using std::string;
using std::vector;

// static
void CommandLineHelper::AddEnabledFeature(base::CommandLine& command_line,
                                          const string& feature_name) {
  const string enabled_features_list =
      command_line.GetSwitchValueASCII(switches::kEnableFeatures);
  const string disabled_features_list =
      command_line.GetSwitchValueASCII(switches::kDisableFeatures);

  if (enabled_features_list.empty() && disabled_features_list.empty()) {
    command_line.AppendSwitchASCII(switches::kEnableFeatures, feature_name);
    return;
  }

  vector<string> enabled_features =
      base::FeatureList::SplitFeatureListString(enabled_features_list);
  vector<string> disabled_features =
      base::FeatureList::SplitFeatureListString(disabled_features_list);

  if (!base::ContainsValue(enabled_features, feature_name) &&
      !base::ContainsValue(disabled_features, feature_name)) {
    enabled_features.push_back(feature_name);
    command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                   base::JoinString(enabled_features, ","));
  }
}
