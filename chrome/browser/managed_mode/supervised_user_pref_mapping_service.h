// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_SUPERVISED_USER_PREF_MAPPING_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_SUPERVISED_USER_PREF_MAPPING_SERVICE_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/keyed_service/core/keyed_service.h"

class ManagedUserSharedSettingsService;
class PrefService;

// SupervisedUserPrefMappingService maps shared managed user settings to user
// preferences. When a shared managed user setting is updated via sync, the
// corresponding local user preference is set to this new value.
class SupervisedUserPrefMappingService : public KeyedService {
 public:
  typedef base::CallbackList<void(const std::string&, const std::string&)>
      CallbackList;

  SupervisedUserPrefMappingService(
      PrefService* prefs,
      ManagedUserSharedSettingsService* shared_settings);
  virtual ~SupervisedUserPrefMappingService();

  // KeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  void Init();

  // Updates the managed user shared setting when the avatar has changed.
  void OnAvatarChanged();

  // Called when a managed user shared setting was changed by receiving new sync
  // data. Updates the corresponding user pref.
  void OnSharedSettingChanged(const std::string& mu_id, const std::string& key);

 private:
  // Returns the current chrome avatar index that is stored as a managed user
  // shared setting, or -1 if no avatar index is stored.
  int GetChromeAvatarIndex();

  PrefService* prefs_;
  ManagedUserSharedSettingsService* shared_settings_;
  scoped_ptr<CallbackList::Subscription> subscription_;
  std::string managed_user_id_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<SupervisedUserPrefMappingService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserPrefMappingService);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_SUPERVISED_USER_PREF_MAPPING_SERVICE_H_
