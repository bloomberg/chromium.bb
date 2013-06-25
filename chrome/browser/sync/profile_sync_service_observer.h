// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_OBSERVER_H_

// Various UI components such as the New Tab page can be driven by observing
// the ProfileSyncService through this interface.
class ProfileSyncServiceObserver {
 public:
  // When one of the following events occurs, OnStateChanged() is called.
  // Observers should query the service to determine what happened.
  // - We initialized successfully.
  // - There was an authentication error and the user needs to reauthenticate.
  // - The sync servers are unavailable at this time.
  // - Credentials are now in flight for authentication.
  virtual void OnStateChanged() = 0;
 protected:
  virtual ~ProfileSyncServiceObserver() { }
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_OBSERVER_H_
