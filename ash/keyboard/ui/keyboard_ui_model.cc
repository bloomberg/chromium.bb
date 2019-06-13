// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_model.h"

#include <set>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"

namespace keyboard {

namespace {

// Returns whether a given state transition is valid.
// See the design document linked in https://crbug.com/71990.
bool IsAllowedStateTransition(KeyboardUIState from, KeyboardUIState to) {
  static const base::NoDestructor<
      std::set<std::pair<KeyboardUIState, KeyboardUIState>>>
      kAllowedStateTransition({
          // The initial ShowKeyboard scenario
          // INITIAL -> LOADING_EXTENSION -> HIDDEN -> SHOWN.
          {KeyboardUIState::kInitial, KeyboardUIState::kLoading},
          {KeyboardUIState::kLoading, KeyboardUIState::kHidden},
          {KeyboardUIState::kHidden, KeyboardUIState::kShown},

          // Hide scenario
          // SHOWN -> WILL_HIDE -> HIDDEN.
          {KeyboardUIState::kShown, KeyboardUIState::kWillHide},
          {KeyboardUIState::kWillHide, KeyboardUIState::kHidden},

          // Focus transition scenario
          // SHOWN -> WILL_HIDE -> SHOWN.
          {KeyboardUIState::kWillHide, KeyboardUIState::kShown},

          // HideKeyboard can be called at anytime for example on shutdown.
          {KeyboardUIState::kShown, KeyboardUIState::kHidden},

          // Return to INITIAL when keyboard is disabled.
          {KeyboardUIState::kLoading, KeyboardUIState::kInitial},
          {KeyboardUIState::kHidden, KeyboardUIState::kInitial},
      });
  return kAllowedStateTransition->count(std::make_pair(from, to)) == 1;
}

// Records a state transition for metrics.
void RecordStateTransition(KeyboardUIState prev, KeyboardUIState next) {
  const bool valid_transition = IsAllowedStateTransition(prev, next);

  // Use negative hash values to indicate invalid transitions.
  const int hash = GetStateTransitionHash(prev, next);
  base::UmaHistogramSparse("VirtualKeyboard.ControllerStateTransition",
                           valid_transition ? hash : -hash);

  DCHECK(valid_transition) << "State: " << StateToStr(prev) << " -> "
                           << StateToStr(next) << " Unexpected transition";
}

}  // namespace

std::string StateToStr(KeyboardUIState state) {
  switch (state) {
    case KeyboardUIState::kUnknown:
      return "UNKNOWN";
    case KeyboardUIState::kInitial:
      return "INITIAL";
    case KeyboardUIState::kLoading:
      return "LOADING";
    case KeyboardUIState::kShown:
      return "SHOWN";
    case KeyboardUIState::kWillHide:
      return "WILL_HIDE";
    case KeyboardUIState::kHidden:
      return "HIDDEN";
  }
}

int GetStateTransitionHash(KeyboardUIState prev, KeyboardUIState next) {
  // The hashes correspond to the KeyboardControllerStateTransition entry in
  // tools/metrics/enums.xml.
  return static_cast<int>(prev) * 1000 + static_cast<int>(next);
}

KeyboardUIModel::KeyboardUIModel() = default;

void KeyboardUIModel::ChangeState(KeyboardUIState new_state) {
  RecordStateTransition(state_, new_state);

  if (new_state == state_)
    return;

  state_ = new_state;
}

}  // namespace keyboard
