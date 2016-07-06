// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/common/search/search_types.h"

class SearchModelObserver;

// Represents whether a page supports Instant.
enum InstantSupportState {
  INSTANT_SUPPORT_NO,
  INSTANT_SUPPORT_YES,
  INSTANT_SUPPORT_UNKNOWN,
};

// An observable model for UI components that care about search model state
// changes.
class SearchModel {
 public:
  struct State {
    State();
    State(const SearchMode& mode, InstantSupportState instant_support);

    bool operator==(const State& rhs) const;

    // The display mode of UI elements such as the toolbar, the tab strip, etc.
    SearchMode mode;

    // Does the current page support Instant?
    InstantSupportState instant_support;
  };

  SearchModel();
  ~SearchModel();

  // Change the state.  Change notifications are sent to observers.
  void SetState(const State& state);

  // Get the current state.
  const State& state() const { return state_; }

  // Change the mode.  Change notifications are sent to observers.
  void SetMode(const SearchMode& mode);

  // Get the active mode.
  const SearchMode& mode() const { return state_.mode; }

  // Sets the page instant support state. Change notifications are sent to
  // observers.
  void SetInstantSupportState(InstantSupportState instant_support);

  // Gets the instant support state of the page.
  InstantSupportState instant_support() const {
    return state_.instant_support;
  }

  // Add and remove observers.
  void AddObserver(SearchModelObserver* observer);
  void RemoveObserver(SearchModelObserver* observer);

 private:
  // Current state of model.
  State state_;

  // Observers.
  base::ObserverList<SearchModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchModel);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
