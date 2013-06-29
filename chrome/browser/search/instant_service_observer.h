// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_

#include <vector>

struct InstantMostVisitedItem;
struct ThemeBackgroundInfo;

// InstantServiceObserver defines the observer interface for InstantService.
class InstantServiceObserver {
 public:
  // Indicates that the user's custom theme has changed in some way.
  virtual void ThemeInfoChanged(const ThemeBackgroundInfo&) = 0;

  // Indicates that the most visited items has changed.
  virtual void MostVisitedItemsChanged(
      const std::vector<InstantMostVisitedItem>&) = 0;

 protected:
  virtual ~InstantServiceObserver() {}
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_OBSERVER_H_
