// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/features.h"

#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

const base::Feature kAssistantVoiceMatch{"AssistantVoiceMatch",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantWarmerWelcomeFeature{
    "AssistantWarmerWelcome", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantAppSupport{"AssistantAppSupport",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantRoutines{"AssistantRoutines",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInAssistantNotifications{
    "InAssistantNotifications", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableClearCutLog{"EnableClearCutLog",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDspHotword{"EnableDspHotword",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStereoAudioInput{"AssistantEnableStereoAudioInput",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTimerNotification{"ChromeOSAssistantTimerNotification",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableTextQueriesWithClientDiscourseContext{
    "AssistantEnableTextQueriesWithClientDiscourseContext",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTimerTicks{"ChromeOSAssistantTimerTicks",
                                base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAppSupportEnabled() {
  return base::FeatureList::IsEnabled(
      assistant::features::kAssistantAppSupport);
}

bool IsClearCutLogEnabled() {
  return base::FeatureList::IsEnabled(kEnableClearCutLog);
}

bool IsDspHotwordEnabled() {
  return base::FeatureList::IsEnabled(kEnableDspHotword);
}

bool IsInAssistantNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kInAssistantNotifications);
}

bool IsRoutinesEnabled() {
  return base::FeatureList::IsEnabled(kAssistantRoutines);
}

bool IsStereoAudioInputEnabled() {
  return base::FeatureList::IsEnabled(kEnableStereoAudioInput);
}

bool IsTimerNotificationEnabled() {
  return base::FeatureList::IsEnabled(kTimerNotification);
}

bool IsTimerTicksEnabled() {
  return base::FeatureList::IsEnabled(kTimerTicks);
}

bool IsWarmerWelcomeEnabled() {
  return base::FeatureList::IsEnabled(kAssistantWarmerWelcomeFeature);
}

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
