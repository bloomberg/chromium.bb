// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/common/search_types.h"

class SearchModelObserver;

// An observable model for UI components that care about search model state
// changes.
class SearchModel {
 public:
  struct State {
    State() : top_bars_visible(true) {}

    bool operator==(const State& rhs) const {
      return mode == rhs.mode && top_bars_visible == rhs.top_bars_visible;
    }

    // The display mode of UI elements such as the toolbar, the tab strip, etc.
    SearchMode mode;

    // The visibility of top bars such as bookmark and info bars.
    bool top_bars_visible;
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
