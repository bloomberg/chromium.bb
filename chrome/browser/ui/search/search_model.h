// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/common/search_types.h"

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
    State(const SearchMode& mode,
          bool top_bars_visible,
          InstantSupportState instant_support,
          bool voice_search_supported);

    bool operator==(const State& rhs) const;

    // The display mode of UI elements such as the toolbar, the tab strip, etc.
    SearchMode mode;

    // The visibility of top bars such as bookmark and info bars.
    bool top_bars_visible;

    // Does the current page support Instant?
    InstantSupportState instant_support;

    // Does the current page support voice search?
    bool voice_search_supported;
  };

  SearchModel();
  ~SearchModel();

  // Returns true if visibility in top bars should be changed based on
  // |old_state| and |new_state|.
  static bool ShouldChangeTopBarsVisibility(const State& old_state,
                                            const State& new_state);

  // Change the state.  Change notifications are sent to observers.
  void SetState(const State& state);

  // Get the current state.
  const State& state() const { return state_; }

  // Change the mode.  Change notifications are sent to observers.
  void SetMode(const SearchMode& mode);

  // Get the active mode.
  const SearchMode& mode() const { return state_.mode; }

  // Set visibility of top bars.  Change notifications are sent to observers.
  void SetTopBarsVisible(bool visible);

  // Get the visibility of top bars.
  bool top_bars_visible() const { return state_.top_bars_visible; }

  // Sets the page instant support state. Change notifications are sent to
  // observers.
  void SetInstantSupportState(InstantSupportState instant_support);

  // Gets the instant support state of the page.
  InstantSupportState instant_support() const {
    return state_.instant_support;
  }

  // Sets the page voice search support state.  Change notifications are sent to
  // observers.
  void SetVoiceSearchSupported(bool supported);

  // Gets the voice search support state of the page.
  bool voice_search_supported() const { return state_.voice_search_supported; }

  // Add and remove observers.
  void AddObserver(SearchModelObserver* observer);
  void RemoveObserver(SearchModelObserver* observer);

 private:
  // Current state of model.
  State state_;

  // Observers.
  ObserverList<SearchModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchModel);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
