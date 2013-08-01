// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SYNC_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SYNC_SERVICE_OBSERVER_H_

class ManagedUserSyncServiceObserver {
 public:
  // Called when the Sync server has acknowledged a newly
  // created managed user.
  virtual void OnManagedUserAcknowledged(
      const std::string& managed_user_id) = 0;

  virtual void OnManagedUsersSyncingStopped() = 0;

 protected:
  virtual ~ManagedUserSyncServiceObserver() {}
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SYNC_SERVICE_OBSERVER_H_
