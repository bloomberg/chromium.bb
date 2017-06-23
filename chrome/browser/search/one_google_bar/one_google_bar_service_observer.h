// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_

// Observer for OneGoogleBarService.
class OneGoogleBarServiceObserver {
 public:
  // Called when the OneGoogleBarData changes, including changes between null
  // and non-null. You can get the new data via
  // OneGoogleBarService::one_google_bar_data().
  virtual void OnOneGoogleBarDataChanged() = 0;

  // Called when an attempt to fetch the OneGoogleBarData failed. Note that
  // there may still be cached data from a previous fetch. If there was cached
  // data before the failed fetch attempt and it got cleared, then
  // OnOneGoogleBarDataChanged gets called first.
  virtual void OnOneGoogleBarFetchFailed() {}

  // Called when the OneGoogleBarService is shutting down. Observers that might
  // outlive the service should use this to unregister themselves, and clear out
  // any pointers to the service they might hold.
  virtual void OnOneGoogleBarServiceShuttingDown() {}
};

#endif  // CHROME_BROWSER_SEARCH_ONE_GOOGLE_BAR_ONE_GOOGLE_BAR_SERVICE_OBSERVER_H_
