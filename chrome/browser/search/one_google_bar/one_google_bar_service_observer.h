// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_

// Observer for OneGoogleBarService.
class OneGoogleBarServiceObserver {
 public:
  // Called when the OneGoogleBarData changes. You can get the new data via
  // OneGoogleBarService::one_google_bar_data().
  virtual void OnOneGoogleBarDataChanged() = 0;
};

#endif  // CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_
