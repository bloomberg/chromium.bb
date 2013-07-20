// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/search_model_observer.h"

SearchModel::State::State()
    : instant_support(INSTANT_SUPPORT_UNKNOWN),
      voice_search_supported(false) {
}

SearchModel::State::State(const SearchMode& mode,
                          InstantSupportState instant_support,
                          bool voice_search_supported)
    : mode(mode),
      instant_support(instant_support),
      voice_search_supported(voice_search_supported) {
}

bool SearchModel::State::operator==(const State& rhs) const {
  return mode == rhs.mode && instant_support == rhs.instant_support &&
      voice_search_supported == rhs.voice_search_supported;
}

SearchModel::SearchModel() {
}

SearchModel::~SearchModel() {
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
