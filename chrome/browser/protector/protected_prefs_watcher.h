// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_PROTECTED_PREFS_WATCHER_H_
#define CHROME_BROWSER_PROTECTOR_PROTECTED_PREFS_WATCHER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "content/public/browser/notification_observer.h"

class PrefService;
class PrefSetObserver;
class Profile;

namespace base {
class Value;
}

namespace protector {

class ProtectedPrefsWatcher : public content::NotificationObserver {
 public:
  // Current backup version.
  static const int kCurrentVersionNumber;

  explicit ProtectedPrefsWatcher(Profile* profile);
  virtual ~ProtectedPrefsWatcher();

  // Registers prefs on a new Profile instance.
  static void RegisterUserPrefs(PrefService* prefs);

  // Returns true if pref named |path| has changed and the backup is valid.
  bool DidPrefChange(const std::string& path) const;

  // Returns the backup value for pref named |path| or |NULL| if the pref is not
  // protected, does not exist or the backup is invalid. The returned Value
  // instance is owned by the PrefService.
  const base::Value* GetBackupForPref(const std::string& path) const;

  // Forces a valid backup, matching actual preferences (overwriting any
  // previous data). Should only be when protector service is disabled.
  void ForceUpdateBackup();

  // True if the backup was valid at the profile load time.
  bool is_backup_valid() { return is_backup_valid_; }

 private:
  friend class ProtectedPrefsWatcherTest;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Makes sure that all protected prefs have been migrated before starting to
  // observe them.
  void EnsurePrefsMigration();

  // Updates cached prefs from their actual values and returns |true| if there
  // were any changes.
  bool UpdateCachedPrefs();

  // Returns |false| if profile does not have a backup yet (needs to be
  // initialized).
  bool HasBackup() const;

  // Creates initial backup entries.
  void InitBackup();

  // Migrates backup if it is an older version.
  void MigrateOldBackupIfNeeded();

  // Updates the backup entry for |path| and returns |true| if the backup has
  // changed.
  bool UpdateBackupEntry(const std::string& path);

  // Perform a check that backup is valid and settings have not been modified.
  void ValidateBackup();

  // Returns |true| if backup signature is valid.
  bool IsSignatureValid() const;

  // Updates the backup signature.
  void UpdateBackupSignature();

  // Returns data to be signed as string.
  std::string GetSignatureData(PrefService* prefs) const;

  // Cached set of extension IDs. They are not changed as frequently
  ExtensionPrefs::ExtensionIdSet cached_extension_ids_;

  scoped_ptr<PrefSetObserver> pref_observer_;

  // True if the backup was valid at the profile load time.
  bool is_backup_valid_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedPrefsWatcher);
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_PROTECTED_PREFS_WATCHER_H_
