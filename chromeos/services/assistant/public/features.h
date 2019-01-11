// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

// Enables Assistant voice match enrollment.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantVoiceMatch;

// Enable Assistant warmer welcome.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantWarmerWelcomeFeature;

// Enables Assistant app support.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAppSupport;

// Enables DSP for hotword detection.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableDspHotword;

// Enables stereo audio input.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableStereoAudioInput;

// Enables timer notifications.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kTimerNotification;

// Enables timer ticks. This feature causes alarms/timers tracked by the
// AssistantAlarmTimerController to tick at a fixed interval, delivering updates
// to AssistantAlarmTimerModelObservers of time remaining/elapsed since expiry.
// When enabled in conjunction with |kTimerNotification|, Assistant alarm/timer
// notifications will be updated at each tick.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kTimerTicks;

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsDspHotwordEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsStereoAudioInputEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsTimerNotificationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsTimerTicksEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsWarmerWelcomeEnabled();

// Enables sending the client discourse context with text queries.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableTextQueriesWithClientDiscourseContext;

}  // namespace features
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
