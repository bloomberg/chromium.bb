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
COMPONENT_EXPORT(ASSISTANT_SERVICE)
extern const base::Feature kAssistantVoiceMatch;

// Enable Assistant warmer welcome.
COMPONENT_EXPORT(ASSISTANT_SERVICE)
extern const base::Feature kAssistantWarmerWelcomeFeature;

// Enables Assistant app support.
COMPONENT_EXPORT(ASSISTANT_SERVICE)
extern const base::Feature kAssistantAppSupport;

// Enables DSP for hotword detection.
COMPONENT_EXPORT(ASSISTANT_SERVICE)
extern const base::Feature kEnableDspHotword;

// Enables stereo audio input.
COMPONENT_EXPORT(ASSISTANT_SERVICE)
extern const base::Feature kEnableStereoAudioInput;

// Enables timer notifications.
COMPONENT_EXPORT(ASSISTANT_SERVICE)
extern const base::Feature kTimerNotification;

COMPONENT_EXPORT(ASSISTANT_SERVICE) bool IsDspHotwordEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE) bool IsStereoAudioInputEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE) bool IsTimerNotificationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE) bool IsWarmerWelcomeEnabled();

}  // namespace features
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
