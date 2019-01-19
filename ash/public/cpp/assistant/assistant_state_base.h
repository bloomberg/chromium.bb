// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_

#include <string>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ash {

// Plain data class that holds Assistant related prefs and states. This is
// shared by both the controller that controlls these values and client proxy
// that caches these values locally. Please do not use this object directly,
// most likely you want to use |AssistantStateProxy|.
class ASH_PUBLIC_EXPORT AssistantStateBase {
 public:
  AssistantStateBase();
  virtual ~AssistantStateBase();

  const base::Optional<mojom::VoiceInteractionState>& voice_interaction_state()
      const {
    return voice_interaction_state_;
  }

  const base::Optional<bool>& settings_enabled() const {
    return settings_enabled_;
  }

  const base::Optional<bool>& setup_completed() const {
    return setup_completed_;
  }

  const base::Optional<bool>& context_enabled() const {
    return context_enabled_;
  }

  const base::Optional<bool>& hotword_enabled() const {
    return hotword_enabled_;
  }

  const base::Optional<bool>& hotword_always_on() const {
    return hotword_always_on_;
  }

  const base::Optional<mojom::AssistantAllowedState>& allowed_state() const {
    return allowed_state_;
  }

  const base::Optional<std::string>& locale() const { return locale_; }

 protected:
  base::Optional<mojom::VoiceInteractionState> voice_interaction_state_;

  // Whether voice interaction is enabled in system settings.
  base::Optional<bool> settings_enabled_;

  // Whether voice interaction setup flow has completed.
  base::Optional<bool> setup_completed_;

  // Whether screen context is enabled.
  base::Optional<bool> context_enabled_;

  // Whether hotword listening is enabled.
  base::Optional<bool> hotword_enabled_;

  // Whether hotword listening is always on/only with power source.
  base::Optional<bool> hotword_always_on_;

  // Whether voice interaction feature is allowed or disallowed for what reason.
  base::Optional<mojom::AssistantAllowedState> allowed_state_;

  base::Optional<std::string> locale_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantStateBase);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_
