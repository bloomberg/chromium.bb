// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_H_

#include <string>

#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"

namespace syncer {

// This class encapsulates all the user-configurable bits of Sync.
class SyncUserSettings {
 public:
  virtual ~SyncUserSettings() = default;

  // Whether the user wants Sync to run, a.k.a. the Sync feature toggle in
  // settings. This maps to DISABLE_REASON_USER_CHOICE.
  // NOTE: This is true by default, even if the user has never enabled Sync or
  // isn't even signed in.
  virtual bool IsSyncRequested() const = 0;
  virtual void SetSyncRequested(bool requested) = 0;

  // Whether Sync is allowed at the platform level (e.g. Android's "MasterSync"
  // toggle). Maps to DISABLE_REASON_PLATFORM_OVERRIDE.
  virtual bool IsSyncAllowedByPlatform() const = 0;
  virtual void SetSyncAllowedByPlatform(bool allowed) = 0;

  // Whether the initial Sync setup has been completed, meaning the user has
  // consented to Sync.
  // NOTE: On Android and ChromeOS, this gets set automatically, so it doesn't
  // really mean anything. See |browser_defaults::kSyncAutoStarts|.
  virtual bool IsFirstSetupComplete() const = 0;
  virtual void SetFirstSetupComplete() = 0;

  // The user's chosen data types. The "sync everything" flag means to sync all
  // current and future data types. If it is set, then GetChosenDataTypes() will
  // always return "all types". The chosen types are always a subset of
  // syncer::UserSelectableTypes().
  // NOTE: By default, GetChosenDataTypes() returns "all types", even if the
  // user has never enabled Sync, or if only Sync-the-transport is running.
  virtual bool IsSyncEverythingEnabled() const = 0;
  virtual syncer::ModelTypeSet GetChosenDataTypes() const = 0;
  virtual void SetChosenDataTypes(bool sync_everything,
                                  syncer::ModelTypeSet types) = 0;

  // Encryption.
  virtual bool IsEncryptEverythingAllowed() const = 0;
  virtual void SetEncryptEverythingAllowed(bool allowed) = 0;
  virtual bool IsEncryptEverythingEnabled() const = 0;
  virtual void EnableEncryptEverything() = 0;

  virtual bool IsPassphraseRequired() const = 0;
  virtual bool IsPassphraseRequiredForDecryption() const = 0;
  // "Secondary" means either a custom or a frozen implicit passphrase.
  virtual bool IsUsingSecondaryPassphrase() const = 0;
  virtual base::Time GetExplicitPassphraseTime() const = 0;
  virtual syncer::PassphraseType GetPassphraseType() const = 0;

  virtual void SetEncryptionPassphrase(const std::string& passphrase) = 0;
  virtual bool SetDecryptionPassphrase(const std::string& passphrase) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_H_
