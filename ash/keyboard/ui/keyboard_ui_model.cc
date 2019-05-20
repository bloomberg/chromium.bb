// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_model.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"

namespace keyboard {

namespace {

// Returns whether a given state transition is valid.
// See the design document linked in https://crbug.com/71990.
bool IsAllowedStateTransition(KeyboardControllerState from,
                              KeyboardControllerState to) {
  static const base::NoDestructor<
      std::set<std::pair<KeyboardControllerState, KeyboardControllerState>>>
      kAllowedStateTransition({
          // The initial ShowKeyboard scenario
          // INITIAL -> LOADING_EXTENSION -> HIDDEN -> SHOWN.
          {KeyboardControllerState::kInitial,
           KeyboardControllerState::kLoadingExtension},
          {KeyboardControllerState::kLoadingExtension,
           KeyboardControllerState::kHidden},
          {KeyboardControllerState::kHidden, KeyboardControllerState::kShown},

          // Hide scenario
          // SHOWN -> WILL_HIDE -> HIDDEN.
          {KeyboardControllerState::kShown, KeyboardControllerState::kWillHide},
          {KeyboardControllerState::kWillHide,
           KeyboardControllerState::kHidden},

          // Focus transition scenario
          // SHOWN -> WILL_HIDE -> SHOWN.
          {KeyboardControllerState::kWillHide, KeyboardControllerState::kShown},

          // HideKeyboard can be called at anytime for example on shutdown.
          {KeyboardControllerState::kShown, KeyboardControllerState::kHidden},

          // Return to INITIAL when keyboard is disabled.
          {KeyboardControllerState::kLoadingExtension,
           KeyboardControllerState::kInitial},
          {KeyboardControllerState::kHidden, KeyboardControllerState::kInitial},
      });
  return kAllowedStateTransition->count(std::make_pair(from, to)) == 1;
}

// Records a state transition for metrics.
void RecordStateTransition(KeyboardControllerState prev,
                           KeyboardControllerState next) {
  const bool valid_transition = IsAllowedStateTransition(prev, next);

  // Emit UMA
  const int transition_record =
      (valid_transition ? 1 : -1) *
      (static_cast<int>(prev) * 1000 + static_cast<int>(next));
  base::UmaHistogramSparse("VirtualKeyboard.ControllerStateTransition",
                           transition_record);
  UMA_HISTOGRAM_BOOLEAN("VirtualKeyboard.ControllerStateTransitionIsValid",
                        valid_transition);

  DCHECK(valid_transition) << "State: " << StateToStr(prev) << " -> "
                           << StateToStr(next) << " Unexpected transition";
}

}  // namespace

std::string StateToStr(KeyboardControllerState state) {
  switch (state) {
    case KeyboardControllerState::kUnknown:
      return "UNKNOWN";
    case KeyboardControllerState::kInitial:
      return "INITIAL";
    case KeyboardControllerState::kLoadingExtension:
      return "LOADING_EXTENSION";
    case KeyboardControllerState::kShown:
      return "SHOWN";
    case KeyboardControllerState::kWillHide:
      return "WILL_HIDE";
    case KeyboardControllerState::kHidden:
      return "HIDDEN";
  }
}

KeyboardUIModel::KeyboardUIModel() = default;

void KeyboardUIModel::ChangeState(KeyboardControllerState new_state) {
  RecordStateTransition(state_, new_state);

  if (new_state == state_)
    return;

  state_ = new_state;
}

}  // namespace keyboard
