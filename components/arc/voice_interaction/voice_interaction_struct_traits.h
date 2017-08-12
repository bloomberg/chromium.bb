// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_STRUCT_TRAITS_H_
#define COMPONENTS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_STRUCT_TRAITS_H_

#include "ash/public/cpp/voice_interaction_state.h"
#include "components/arc/common/voice_interaction_framework.mojom.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::VoiceInteractionState,
                  ash::VoiceInteractionState> {
  static arc::mojom::VoiceInteractionState ToMojom(
      ash::VoiceInteractionState state) {
    switch (state) {
      case ash::VoiceInteractionState::NOT_READY:
        return arc::mojom::VoiceInteractionState::NOT_READY;
      case ash::VoiceInteractionState::STOPPED:
        return arc::mojom::VoiceInteractionState::STOPPED;
      case ash::VoiceInteractionState::RUNNING:
        return arc::mojom::VoiceInteractionState::RUNNING;
    }

    NOTREACHED() << "Invalid state: " << static_cast<int>(state);
    return arc::mojom::VoiceInteractionState::NOT_READY;
  }

  static bool FromMojom(arc::mojom::VoiceInteractionState mojom_state,
                        ash::VoiceInteractionState* state) {
    switch (mojom_state) {
      case arc::mojom::VoiceInteractionState::NOT_READY:
        *state = ash::VoiceInteractionState::NOT_READY;
        return true;
      case arc::mojom::VoiceInteractionState::STOPPED:
        *state = ash::VoiceInteractionState::STOPPED;
        return true;
      case arc::mojom::VoiceInteractionState::RUNNING:
        *state = ash::VoiceInteractionState::RUNNING;
        return true;
    }

    NOTREACHED() << "Invalid state: " << static_cast<int>(mojom_state);
    *state = ash::VoiceInteractionState::NOT_READY;
    return false;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_VOICE_INTERACTION_VOICE_INTERACTION_STRUCT_TRAITS_H_
