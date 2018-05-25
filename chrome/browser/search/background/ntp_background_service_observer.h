// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_OBSERVER_H_

// Observer for NtpBackgroundService.
class NtpBackgroundServiceObserver {
 public:
  // Called when the CollectionInfoData is updated, usually as the result of a
  // FetchCollectionInfo() call on the service. Note that this is called after
  // each FetchCollectionInfo(), even if the network request failed, or if it
  // didn't result in an actual change to the cached data. You can get the new
  // data via NtpBackgroundService::collection_info_data().
  virtual void OnCollectionInfoAvailable() = 0;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_OBSERVER_H_
