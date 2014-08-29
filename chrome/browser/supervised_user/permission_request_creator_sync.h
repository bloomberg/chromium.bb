// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_SYNC_H_
#define CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_SYNC_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"

class SupervisedUserSettingsService;
class SupervisedUserSharedSettingsService;

class PermissionRequestCreatorSync : public PermissionRequestCreator {
 public:
  PermissionRequestCreatorSync(
      SupervisedUserSettingsService* settings_service,
      SupervisedUserSharedSettingsService* shared_settings_service,
      const std::string& name,
      const std::string& supervised_user_id);
  virtual ~PermissionRequestCreatorSync();

  // PermissionRequestCreator implementation:
  virtual void CreatePermissionRequest(const GURL& url_requested,
                                       const base::Closure& callback) OVERRIDE;

 private:
  SupervisedUserSettingsService* settings_service_;
  SupervisedUserSharedSettingsService* shared_settings_service_;
  std::string name_;
  std::string supervised_user_id_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_SYNC_H_
