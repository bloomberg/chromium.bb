// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/common/search/search_types.h"

class SearchModelObserver;

// An observable model for UI components that care about search model state
// changes.
class SearchModel {
 public:
  SearchModel();
  ~SearchModel();

  // Change the mode.  Change notifications are sent to observers.
  void SetMode(const SearchMode& mode);

  // Get the active mode.
  const SearchMode& mode() const { return mode_; }

  // Add and remove observers.
  void AddObserver(SearchModelObserver* observer);
  void RemoveObserver(SearchModelObserver* observer);

 private:
  // Current state of model.
  SearchMode mode_;

  // Observers.
  base::ObserverList<SearchModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchModel);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
