// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_H_
#define IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_user_settings.h"

namespace syncer {
class SyncService;
class SyncSetupInProgressHandle;
}  // namespace syncer

// Class that allows configuring sync. It handles enabling and disabling it, as
// well as choosing datatypes. Most actions are delayed until a commit is done,
// to allow the complex sync setup flow on iOS.
class SyncSetupService : public KeyedService {
 public:
  using SyncServiceState = enum {
    kNoSyncServiceError,
    kSyncServiceSignInNeedsUpdate,
    kSyncServiceCouldNotConnect,
    kSyncServiceServiceUnavailable,
    kSyncServiceNeedsPassphrase,
    kSyncServiceNeedsTrustedVaultKey,
    kSyncServiceUnrecoverableError,
    kSyncSettingsNotConfirmed,
    kLastSyncServiceError = kSyncServiceUnrecoverableError
  };

  // The set of user-selectable datatypes handled by Chrome for iOS.
  using SyncableDatatype = enum {
    kSyncBookmarks,
    kSyncOmniboxHistory,
    kSyncPasswords,
    kSyncOpenTabs,
    kSyncAutofill,
    kSyncPreferences,
    kSyncReadingList,
    kNumberOfSyncableDatatypes
  };

  explicit SyncSetupService(syncer::SyncService* sync_service);
  ~SyncSetupService() override;

  // Returns the |syncer::ModelType| associated to the given
  // |SyncableDatatypes|.
  syncer::ModelType GetModelType(SyncableDatatype datatype);

  // Returns whether sync is enabled.
  virtual bool IsSyncEnabled() const;
  // Enables or disables sync. Changes won't take effect in the sync backend
  // before the next call to |CommitChanges|.
  virtual void SetSyncEnabled(bool sync_enabled);

  // Returns all currently enabled datatypes.
  syncer::ModelTypeSet GetPreferredDataTypes() const;
  // Returns whether the given datatype has been enabled for sync and its
  // initialization is complete (SyncEngineHost::OnEngineInitialized has been
  // called).
  virtual bool IsDataTypeActive(syncer::ModelType datatype) const;
  // Returns whether the given datatype is enabled by the user.
  virtual bool IsDataTypePreferred(syncer::ModelType datatype) const;
  // Enables or disables the given datatype. To be noted: this can be called at
  // any time, but will only be meaningful if |IsSyncEnabled| is true and
  // |IsSyncingAllDataTypes| is false. Changes won't take effect in the sync
  // backend before the next call to |CommitChanges|.
  void SetDataTypeEnabled(syncer::ModelType datatype, bool enabled);

  // Returns whether the user needs to enter a passphrase or enable sync to make
  // tab sync work.
  bool UserActionIsRequiredToHaveTabSyncWork();

  // Returns whether all datatypes are being synced.
  virtual bool IsSyncingAllDataTypes() const;
  // Sets whether all datatypes should be synced or not. Changes won't take
  // effect before the next call to |CommitChanges|.
  virtual void SetSyncingAllDataTypes(bool sync_all);

  // Returns the current sync service state.
  virtual SyncServiceState GetSyncServiceState();

  // Returns whether all sync data is being encrypted.
  virtual bool IsEncryptEverythingEnabled() const;

  // Returns true if the user has gone through the initial sync configuration.
  // This method is guaranteed not to start the sync backend so it can be
  // called at start-up.
  // TODO(crbug.com/951313): This method has to be remove when UnifiedConsent
  // flag is cleaned up.
  virtual bool HasFinishedInitialSetup();

  // Pauses sync allowing the user to configure what data to sync before
  // actually starting to sync data with the server.
  virtual void PrepareForFirstSyncSetup();

  // Sets the first setup complete flag. This method doesn't commit sync
  // changes. PrepareForFirstSyncSetup() needs to be called before. This flag is
  // not set if the user didn't turn on sync.
  // This method should only be used with UnifiedConsent flag.
  void SetFirstSetupComplete(syncer::SyncFirstSetupCompleteSource source);

  // Returns true if the user finished the Sync setup flow.
  bool IsFirstSetupComplete() const;

  // Commits all the pending configuration changes to Sync.
  // This method should only be used with UnifiedConsent flag.
  void CommitSyncChanges();

  // Returns true if there are uncommitted sync changes;
  bool HasUncommittedChanges();

 private:
  // Enables or disables sync. Changes won't take effect in the sync backend
  // before the next call to |CommitChanges|. No changes are made to the
  // currently selected datatypes.
  void SetSyncEnabledWithoutChangingDatatypes(bool sync_enabled);

  syncer::SyncService* const sync_service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupService);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SYNC_SETUP_SERVICE_H_
