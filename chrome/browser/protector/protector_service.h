// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_H_
#define CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/settings_change_global_error_delegate.h"

class GURL;
class PrefService;
class Profile;
class TemplateURLService;

namespace protector {

class ProtectedPrefsWatcher;
class SettingsChangeGlobalError;

// Presents a SettingChange to user and handles possible user actions.
class ProtectorService : public ProfileKeyedService,
                         public SettingsChangeGlobalErrorDelegate {
 public:
  explicit ProtectorService(Profile* profile);
  virtual ~ProtectorService();

  // Shows global error about the specified change. Owns |change|. May be called
  // multiple times in which case subsequent bubbles will be displayed.
  virtual void ShowChange(BaseSettingChange* change);

  // Returns |true| if a change is currently active (shown by a ShowChange call
  // and not yet applied or discarded).
  virtual bool IsShowingChange() const;

  // Removes corresponding global error (including the bubbble if one is shown)
  // and deletes the change instance (without calling Apply or Discard on it).
  virtual void DismissChange(BaseSettingChange* change);

  // Persists |change| and removes corresponding global error. |browser| is the
  // Browser instance from which the user action originates.
  virtual void ApplyChange(BaseSettingChange* change, Browser* browser);

  // Discards |change| and removes corresponding global error. |browser| is the
  // Browser instance from which the user action originates.
  virtual void DiscardChange(BaseSettingChange* change, Browser* browser);

  // Opens a tab with specified URL in the browser window we've shown error
  // bubble for.
  virtual void OpenTab(const GURL& url, Browser* browser);

  // Returns the ProtectedPrefsWatcher instance for access to protected prefs
  // backup.
  ProtectedPrefsWatcher* GetPrefsWatcher();

  // Stops observing pref changes and updating the backup. Should be used in
  // tests only.
  void StopWatchingPrefsForTesting();

  // Returns the most recent change instance or NULL if there are no changes.
  BaseSettingChange* GetLastChange();

  // Returns the Profile instance for this service.
  Profile* profile() { return profile_; }

 private:
  friend class ProtectorServiceTest;

  // Each item consists of an error and corresponding change instance.
  // linked_ptr is used because Item instances are stored in a std::vector and
  // must be copyable.
  struct Item {
    Item();
    ~Item();
    linked_ptr<BaseSettingChange> change;
    linked_ptr<SettingsChangeGlobalError> error;
    // When true, this means |change| was merged with another instance and
    // |error| is in process of being removed from GlobalErrorService.
    bool was_merged;
    // Meaningful only when |was_merged| is true. In that case, true means that
    // the new merged GlobalError instance will be immediately shown.
    bool show_when_merged;
  };

  typedef std::vector<Item> Items;

  // Matches Item by |change| field.
  class MatchItemByChange {
   public:
    explicit MatchItemByChange(const BaseSettingChange* other);

    bool operator()(const Item& item);

   private:
    const BaseSettingChange* other_;
  };

  // Matches Item by |error| field.
  class MatchItemByError {
   public:
    explicit MatchItemByError(const SettingsChangeGlobalError* other);

    bool operator()(const Item& item);

   private:
    const SettingsChangeGlobalError* other_;
  };

  // Returns an Item instance whose change can be merged with |change|, if any.
  // Otherwise returns |NULL|. Provided that the merge strategy is transitive,
  // there can be only one such instance.
  Item* FindItemToMergeWith(const BaseSettingChange* change);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // SettingsChangeGlobalErrorDelegate implementation.
  virtual void OnApplyChange(SettingsChangeGlobalError* error,
                             Browser* browser) OVERRIDE;
  virtual void OnDiscardChange(SettingsChangeGlobalError* error,
                               Browser* browser) OVERRIDE;
  virtual void OnDecisionTimeout(SettingsChangeGlobalError* error) OVERRIDE;
  virtual void OnRemovedFromProfile(SettingsChangeGlobalError* error) OVERRIDE;

  // Pointers to error bubble controllers and corresponding changes in the order
  // added.
  Items items_;

  // Profile which settings we are protecting.
  Profile* profile_;

  // True if there is a change that has been shown and not yet accepted or
  // discarded by user.
  bool has_active_change_;

  // Observes changes to protected prefs and updates the backup.
  scoped_ptr<ProtectedPrefsWatcher> prefs_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ProtectorService);
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_PROTECTOR_SERVICE_H_
