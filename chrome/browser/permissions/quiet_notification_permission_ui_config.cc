// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"

// static
const char QuietNotificationPermissionUiConfig::kEnableAdaptiveActivation[] =
    "enable_adaptive_activation";

QuietNotificationPermissionUiConfig::UiFlavor
QuietNotificationPermissionUiConfig::UiFlavorToUse() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return UiFlavor::NONE;

#if defined(OS_ANDROID)
  return UiFlavor::MINI_INFOBAR;
#else   // OS_ANDROID
  return UiFlavor::ANIMATED_ICON;
#endif  // !OS_ANDROID
}

// static
bool QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() {
  if (!base::FeatureList::IsEnabled(features::kQuietNotificationPrompts))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQuietNotificationPrompts, kEnableAdaptiveActivation,
      false /* default */);
}
