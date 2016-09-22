// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LEGACY_PERMISSION_REQUEST_CREATOR_SYNC_H_
#define CHROME_BROWSER_SUPERVISED_USER_LEGACY_PERMISSION_REQUEST_CREATOR_SYNC_H_

#include <memory>
#include <string>

#include "chrome/browser/supervised_user/permission_request_creator.h"

class SupervisedUserSettingsService;
class SupervisedUserSharedSettingsService;

namespace browser_sync {
class ProfileSyncService;
}  // namespace browser_sync

// The requests are stored using a prefix followed by a URIEncoded version of
// the URL/extension ID. Each entry contains a dictionary which currently has
// the timestamp of the request in it.
class PermissionRequestCreatorSync : public PermissionRequestCreator {
 public:
  PermissionRequestCreatorSync(
      SupervisedUserSettingsService* settings_service,
      SupervisedUserSharedSettingsService* shared_settings_service,
      browser_sync::ProfileSyncService* sync_service,
      const std::string& name,
      const std::string& supervised_user_id);
  ~PermissionRequestCreatorSync() override;

  // PermissionRequestCreator implementation:
  bool IsEnabled() const override;
  void CreateURLAccessRequest(const GURL& url_requested,
                              const SuccessCallback& callback) override;
  void CreateExtensionInstallRequest(const std::string& id,
                                     const SuccessCallback& callback) override;
  void CreateExtensionUpdateRequest(const std::string& id,
                                    const SuccessCallback& callback) override;

 private:
  // Note: Doesn't escape |data|. If you need it escaped, do it yourself!
  void CreateRequest(const std::string& prefix,
                     const std::string& data,
                     const SuccessCallback& callback);
  SupervisedUserSettingsService* settings_service_;
  SupervisedUserSharedSettingsService* shared_settings_service_;
  browser_sync::ProfileSyncService* sync_service_;
  std::string name_;
  std::string supervised_user_id_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LEGACY_PERMISSION_REQUEST_CREATOR_SYNC_H_
