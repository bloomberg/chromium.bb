// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_model.h"

#include "chrome/browser/ui/search/search_model_observer.h"
#include "components/search/search.h"

SearchModel::State::State() : instant_support(INSTANT_SUPPORT_UNKNOWN) {}

SearchModel::State::State(const SearchMode& mode,
                          InstantSupportState instant_support)
    : mode(mode), instant_support(instant_support) {}

bool SearchModel::State::operator==(const State& rhs) const {
  return mode == rhs.mode && instant_support == rhs.instant_support;
}

SearchModel::SearchModel() {
}

SearchModel::~SearchModel() {
}

void SearchModel::SetState(const State& new_state) {
  DCHECK(search::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (state_ == new_state)
    return;

  const State old_state = state_;
  state_ = new_state;

  for (SearchModelObserver& observer : observers_)
    observer.ModelChanged(old_state, state_);
}

void SearchModel::SetMode(const SearchMode& new_mode) {
  DCHECK(search::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel mode without first "
      << "checking if Search is enabled.";

  if (state_.mode == new_mode)
    return;

  const State old_state = state_;
  state_.mode = new_mode;

  for (SearchModelObserver& observer : observers_)
    observer.ModelChanged(old_state, state_);
}

void SearchModel::SetInstantSupportState(InstantSupportState instant_support) {
  DCHECK(search::IsInstantExtendedAPIEnabled())
      << "Please do not try to set the SearchModel state without first "
      << "checking if Search is enabled.";

  if (state_.instant_support == instant_support)
    return;

  const State old_state = state_;
  state_.instant_support = instant_support;
  for (SearchModelObserver& observer : observers_)
    observer.ModelChanged(old_state, state_);
}

void SearchModel::AddObserver(SearchModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchModel::RemoveObserver(SearchModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
