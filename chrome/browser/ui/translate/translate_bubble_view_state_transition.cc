// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

TranslateBubbleViewStateTransition::TranslateBubbleViewStateTransition(
    TranslateBubbleModel::ViewState view_state)
    : view_state_(view_state), view_state_before_advanced_view_(view_state) {
  // The initial view type must not be 'Advanced'.
  DCHECK_NE(TranslateBubbleModel::VIEW_STATE_ADVANCED, view_state_);
}

void TranslateBubbleViewStateTransition::SetViewState(
    TranslateBubbleModel::ViewState view_state) {
  view_state_ = view_state;
  if (view_state != TranslateBubbleModel::VIEW_STATE_ADVANCED)
    view_state_before_advanced_view_ = view_state;
  else
    UMA_HISTOGRAM_ENUMERATION("Translate.BubbleUiEvent",
                              translate::SET_STATE_OPTIONS,
                              translate::TRANSLATE_BUBBLE_UI_EVENT_MAX);
}

void TranslateBubbleViewStateTransition::GoBackFromAdvanced() {
  DCHECK(view_state_ == TranslateBubbleModel::VIEW_STATE_ADVANCED);
  UMA_HISTOGRAM_ENUMERATION("Translate.BubbleUiEvent",
                            translate::LEAVE_STATE_OPTIONS,
                            translate::TRANSLATE_BUBBLE_UI_EVENT_MAX);
  SetViewState(view_state_before_advanced_view_);
}
