// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

// Enables Assistant voice match enrollment.
extern const base::Feature kAssistantVoiceMatch;

// Enables DSP for hotword detection.
extern const base::Feature kEnableDspHotword;

// Enables stereo audio input.
extern const base::Feature kEnableStereoAudioInput;

bool IsDspHotwordEnabled();

bool IsStereoAudioInputEnabled();

}  // namespace features
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_FEATURES_H_
