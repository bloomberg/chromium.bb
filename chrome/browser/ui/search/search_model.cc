// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/search_model_observer.h"

SearchModel::State::State()
    : top_bars_visible(true),
      instant_support(INSTANT_SUPPORT_UNKNOWN),
      voice_search_supported(false) {
}

SearchModel::State::State(const SearchMode& mode,
                          bool top_bars_visible,
                          InstantSupportState instant_support,
                          bool voice_search_supported)
    : mode(mode),
      top_bars_visible(top_bars_visible),
      instant_support(instant_support),
      voice_search_supported(voice_search_supported) {
}

bool SearchModel::State::operator==(const State& rhs) const {
  return mode == rhs.mode && top_bars_visible == rhs.top_bars_visible &&
      instant_support == rhs.instant_support &&
      voice_search_supported == rhs.voice_search_supported;
}

SearchModel::SearchModel() {
}

SearchModel::~SearchModel() {
}

// static.
bool SearchModel::ShouldChangeTopBarsVisibility(const State& old_state,
                                                const State& new_state) {
  // If mode has changed, only change top bars visibility if new mode is not
  // |SEARCH_SUGGESTIONS| or |SEARCH_RESULTS|.  Top bars visibility for
  // these 2 modes is determined when the mode stays the same, and:
  // - for |NTP/SERP| pages: by SearchBox API, or
  // - for |DEFAULT| pages: by platform-specific implementation of
  //   |InstantOverlayController| when it shows/hides the Instant overlay.
  return old_state.mode != new_state.mode ?
      !new_state.mode.is_search() : new_state.mode.is_search();
}

void SearchModel::SetState(const State& new_state) {
  DCHECK(chrome::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (state_ == new_state)
    return;

  const State old_state = state_;
  state_ = new_state;

  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModelChanged(old_state, state_));
}

void SearchModel::SetMode(const SearchMode& new_mode) {
  DCHECK(chrome::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (state_.mode == new_mode)
    return;

  const State old_state = state_;
  state_.mode = new_mode;

  // For |SEARCH_SUGGESTIONS| and |SEARCH_RESULTS| modes, SearchBox API will
  // determine visibility of top bars via SetTopBarsVisible(); for other modes,
  // top bars are always visible, if available.
  if (!state_.mode.is_search())
    state_.top_bars_visible = true;

  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModelChanged(old_state, state_));
}

void SearchModel::SetTopBarsVisible(bool visible) {
  DCHECK(chrome::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel state without first "
      << "checking if Search is enabled.";

  if (state_.top_bars_visible == visible)
    return;

  const State old_state = state_;
  state_.top_bars_visible = visible;

  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModelChanged(old_state, state_));
}

void SearchModel::SetInstantSupportState(InstantSupportState instant_support) {
  DCHECK(chrome::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel state without first "
      << "checking if Search is enabled.";

  if (state_.instant_support == instant_support)
    return;

  const State old_state = state_;
  state_.instant_support = instant_support;
  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModelChanged(old_state, state_));
}

void SearchModel::SetVoiceSearchSupported(bool supported) {
  DCHECK(chrome::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel state without first "
      << "checking if Search is enabled.";

  if (state_.voice_search_supported == supported)
    return;

  const State old_state = state_;
  state_.voice_search_supported = supported;

  FOR_EACH_OBSERVER(SearchModelObserver, observers_,
                    ModelChanged(old_state, state_));
}

void SearchModel::AddObserver(SearchModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchModel::RemoveObserver(SearchModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
