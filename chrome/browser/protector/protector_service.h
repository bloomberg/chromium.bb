// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_H_
#define CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_helpers.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/settings_change_global_error_delegate.h"

class GURL;
class PrefService;
class Profile;
class TemplateURLService;

namespace protector {

class SettingsChangeGlobalError;

// Presents a SettingChange to user and handles possible user actions.
class ProtectorService : public ProfileKeyedService,
                         public SettingsChangeGlobalErrorDelegate {
 public:
  explicit ProtectorService(Profile* profile);
  virtual ~ProtectorService();

  // Shows global error about the specified change. Owns |change|.
  // TODO(ivankr): handle multiple subsequent changes.
  virtual void ShowChange(BaseSettingChange* change);

  // Returns |true| if a change is currently active (shown by a ShowChange call
  // and not yet applied or discarded).
  virtual bool IsShowingChange() const;

  // Removes global error (including the bubbble if one is shown) and deletes
  // the change instance (without calling Apply or Discard on it).
  virtual void DismissChange();

  // Persists the change that is currently active and removes global error.
  virtual void ApplyChange();

  // Discards the change that is currently active and removes global error.
  virtual void DiscardChange();

  // Opens a tab with specified URL in the browser window we've shown error
  // bubble for.
  virtual void OpenTab(const GURL& url);

  // Returns the Profile instance we've shown error bubble for.
  Profile* profile() { return profile_; }

 private:
  friend class ProtectorServiceTest;

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // SettingsChangeGlobalErrorDelegate implementation.
  virtual void OnApplyChange() OVERRIDE;
  virtual void OnDiscardChange() OVERRIDE;
  virtual void OnDecisionTimeout() OVERRIDE;
  virtual void OnRemovedFromProfile() OVERRIDE;

  // Pointer to error bubble controller. Indicates if we're showing change
  // notification to user.
  scoped_ptr<SettingsChangeGlobalError> error_;

  // Setting change which we're showing.
  scoped_ptr<BaseSettingChange> change_;

  // Profile which settings we are protecting.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProtectorService);
};

// Signs string value with protector's key.
std::string SignSetting(const std::string& value);

// Returns true if the signature is valid for the specified key.
bool IsSettingValid(const std::string& value, const std::string& signature);

// Whether the Protector feature is enabled.
bool IsEnabled();

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_H_
