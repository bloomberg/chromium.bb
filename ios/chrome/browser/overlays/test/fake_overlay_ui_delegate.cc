// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/test/fake_overlay_ui_delegate.h"

#include "base/logging.h"

FakeOverlayUIDelegate::FakeOverlayUIDelegate() = default;
FakeOverlayUIDelegate::~FakeOverlayUIDelegate() = default;

FakeOverlayUIDelegate::PresentationState
FakeOverlayUIDelegate::GetPresentationState(web::WebState* web_state) {
  return presentation_states_[web_state];
}

void FakeOverlayUIDelegate::SimulateDismissalForWebState(
    web::WebState* web_state,
    OverlayDismissalReason reason) {
  DCHECK_EQ(PresentationState::kPresented, presentation_states_[web_state]);
  switch (reason) {
    case OverlayDismissalReason::kUserInteraction:
      presentation_states_[web_state] = PresentationState::kUserDismissed;
      break;
    case OverlayDismissalReason::kHiding:
      presentation_states_[web_state] = PresentationState::kHidden;
      break;
    case OverlayDismissalReason::kCancellation:
      presentation_states_[web_state] = PresentationState::kCancelled;
      break;
  }
  std::move(overlay_callbacks_[web_state]).Run(reason);
}

void FakeOverlayUIDelegate::ShowOverlayUIForWebState(
    web::WebState* web_state,
    OverlayModality modality,
    OverlayDismissalCallback callback) {
  presentation_states_[web_state] = PresentationState::kPresented;
  overlay_callbacks_[web_state] = std::move(callback);
}

void FakeOverlayUIDelegate::HideOverlayUIForWebState(web::WebState* web_state,
                                                     OverlayModality modality) {
  SimulateDismissalForWebState(web_state, OverlayDismissalReason::kHiding);
}

void FakeOverlayUIDelegate::CancelOverlayUIForWebState(
    web::WebState* web_state,
    OverlayModality modality) {
  PresentationState& state = presentation_states_[web_state];
  if (state == PresentationState::kPresented) {
    SimulateDismissalForWebState(web_state,
                                 OverlayDismissalReason::kCancellation);
  } else {
    state = PresentationState::kNotPresented;
  }
}
