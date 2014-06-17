// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_SERVICE_OBSERVER_H_

class SupervisedUserSyncServiceObserver {
 public:
  // Called when the Sync server has acknowledged a newly
  // created supervised user.
  virtual void OnSupervisedUserAcknowledged(
      const std::string& supervised_user_id) = 0;

  virtual void OnSupervisedUsersSyncingStopped() = 0;

  virtual void OnSupervisedUsersChanged() = 0;

 protected:
  virtual ~SupervisedUserSyncServiceObserver() {}
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_SERVICE_OBSERVER_H_
