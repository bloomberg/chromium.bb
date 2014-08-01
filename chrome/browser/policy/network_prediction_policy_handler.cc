// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_prediction_policy_handler.h"

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"

namespace policy {

NetworkPredictionPolicyHandler::NetworkPredictionPolicyHandler() {
}

NetworkPredictionPolicyHandler::~NetworkPredictionPolicyHandler() {
}

bool NetworkPredictionPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // Deprecated boolean preference.
  const base::Value* network_prediction_enabled =
      policies.GetValue(key::kDnsPrefetchingEnabled);
  // New enumerated preference.
  const base::Value* network_prediction_options =
      policies.GetValue(key::kNetworkPredictionOptions);

  if (network_prediction_enabled &&
      !network_prediction_enabled->IsType(base::Value::TYPE_BOOLEAN)) {
    errors->AddError(key::kDnsPrefetchingEnabled,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(base::Value::TYPE_BOOLEAN));
  }

  if (network_prediction_options &&
      !network_prediction_options->IsType(base::Value::TYPE_INTEGER)) {
    errors->AddError(key::kNetworkPredictionOptions,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(base::Value::TYPE_INTEGER));
  }

  if (network_prediction_enabled && network_prediction_options) {
    errors->AddError(key::kDnsPrefetchingEnabled,
                     IDS_POLICY_OVERRIDDEN,
                     key::kNetworkPredictionOptions);
  }

  return true;
}

void NetworkPredictionPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // If new preference is managed by policy, apply it to both preferences.
  const base::Value* network_prediction_options =
      policies.GetValue(key::kNetworkPredictionOptions);
  int int_setting;
  if (network_prediction_options &&
      network_prediction_options->GetAsInteger(&int_setting)) {
    prefs->SetInteger(prefs::kNetworkPredictionOptions, int_setting);
    // Be conservative here and only set Enabled if policy says ALWAYS.
    prefs->SetBoolean(
        prefs::kNetworkPredictionEnabled,
        int_setting == chrome_browser_net::NETWORK_PREDICTION_ALWAYS);
    return;
  }

  // If deprecated preference is managed by policy, apply it to both
  // preferences.
  const base::Value* network_prediction_enabled =
      policies.GetValue(key::kDnsPrefetchingEnabled);
  bool bool_setting;
  if (network_prediction_enabled &&
      network_prediction_enabled->GetAsBoolean(&bool_setting)) {
    prefs->SetBoolean(prefs::kNetworkPredictionEnabled, bool_setting);
    // Some predictive network actions, most notably prefetch, used to be
    // hardwired never to run on cellular network.  In order to retain this
    // behavior (unless explicitly overriden by kNetworkPredictionOptions),
    // kNetworkPredictionEnabled = true is translated to
    // kNetworkPredictionOptions = WIFI_ONLY.
    prefs->SetInteger(prefs::kNetworkPredictionOptions,
                      bool_setting
                          ? chrome_browser_net::NETWORK_PREDICTION_WIFI_ONLY
                          : chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }
}

}  // namespace policy
