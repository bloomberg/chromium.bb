// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"

class SearchModelObserver;

// An observable model for UI components that care about search model state
// changes.
class SearchModel {
 public:
  enum class Origin {
    // The user is on some page other than the NTP.
    DEFAULT = 0,

    // The user is on the NTP.
    NTP,
  };

  SearchModel();
  ~SearchModel();

  // Change the origin.  Change notifications are sent to observers.
  void SetOrigin(Origin origin);

  // Get the active origin.
  Origin origin() const { return origin_; }

  // Add and remove observers.
  void AddObserver(SearchModelObserver* observer);
  void RemoveObserver(SearchModelObserver* observer);

 private:
  Origin origin_;

  base::ObserverList<SearchModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SearchModel);
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_H_
