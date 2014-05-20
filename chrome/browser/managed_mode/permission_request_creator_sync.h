// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_PERMISSION_REQUEST_CREATOR_SYNC_H_
#define CHROME_BROWSER_MANAGED_MODE_PERMISSION_REQUEST_CREATOR_SYNC_H_

#include "chrome/browser/managed_mode/permission_request_creator.h"

#include "base/memory/scoped_ptr.h"

class ManagedUserSettingsService;
class ManagedUserSharedSettingsService;
class ManagedUserSigninManagerWrapper;

class PermissionRequestCreatorSync : public PermissionRequestCreator {
 public:
  PermissionRequestCreatorSync(
      ManagedUserSettingsService* settings_service,
      ManagedUserSharedSettingsService* shared_settings_service,
      const std::string& name,
      const std::string& managed_user_id);
  virtual ~PermissionRequestCreatorSync();

  // PermissionRequestCreator implementation:
  virtual void CreatePermissionRequest(const std::string& url_requested,
                                       const base::Closure& callback) OVERRIDE;

 private:
  ManagedUserSettingsService* settings_service_;
  ManagedUserSharedSettingsService* shared_settings_service_;
  std::string name_;
  std::string managed_user_id_;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_PERMISSION_REQUEST_CREATOR_SYNC_H_
