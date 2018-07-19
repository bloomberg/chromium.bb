// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_encryption_handler.h"
#include "components/sync/driver/sync_service_observer.h"

struct AccountInfo;
class GoogleServiceAuthError;
class GURL;

namespace sync_sessions {
class OpenTabsUIDelegate;
}  // namespace sync_sessions

namespace syncer {

class BaseTransaction;
class JsController;
class LocalDeviceInfoProvider;
class GlobalIdMapper;
class ProtocolEventObserver;
class SyncClient;
class SyncCycleSnapshot;
struct SyncTokenStatus;
class TypeDebugInfoObserver;
struct SyncStatus;
struct UserShare;

// Events in ClearServerData flow to be recorded in histogram. Existing
// constants should not be deleted or reordered. New ones shold be added at the
// end, before CLEAR_SERVER_DATA_MAX.
enum ClearServerDataEvents {
  // ClearServerData started after user switched to custom passphrase.
  CLEAR_SERVER_DATA_STARTED,
  // DataTypeManager reported that catchup configuration failed.
  CLEAR_SERVER_DATA_CATCHUP_FAILED,
  // ClearServerData flow restarted after browser restart.
  CLEAR_SERVER_DATA_RETRIED,
  // Success.
  CLEAR_SERVER_DATA_SUCCEEDED,
  // Client received RECET_LOCAL_SYNC_DATA after custom passphrase was enabled
  // on different client.
  CLEAR_SERVER_DATA_RESET_LOCAL_DATA_RECEIVED,
  CLEAR_SERVER_DATA_MAX
};

// UIs that need to prevent Sync startup should hold an instance of this class
// until the user has finished modifying sync settings. This is not an inner
// class of SyncService to enable forward declarations.
class SyncSetupInProgressHandle {
 public:
  // UIs should not construct this directly, but instead call
  // SyncService::GetSetupInProgress().
  explicit SyncSetupInProgressHandle(base::Closure on_destroy);

  ~SyncSetupInProgressHandle();

 private:
  base::Closure on_destroy_;
};

class SyncService : public DataTypeEncryptionHandler, public KeyedService {
 public:
  // The set of reasons due to which Sync can be disabled. Meant to be used as a
  // bitmask.
  enum DisableReason {
    DISABLE_REASON_NONE = 0,
    // Sync is disabled via command-line flag or platform-level override (e.g.
    // Android's "MasterSync" toggle).
    DISABLE_REASON_PLATFORM_OVERRIDE = 1 << 0,
    // Sync is disabled by enterprise policy, either browser policy (through
    // prefs) or account policy received from the Sync server.
    DISABLE_REASON_ENTERPRISE_POLICY = 1 << 1,
    // Sync can't start because there is no authenticated user.
    DISABLE_REASON_NOT_SIGNED_IN = 1 << 2,
    // Sync is suppressed by user choice, either via platform-level toggle (e.g.
    // Android's "ChromeSync" toggle), a “Reset Sync” operation from the
    // dashboard on desktop/ChromeOS.
    // NOTE: Other code paths that go through RequestStop also set this reason
    // (e.g. disabling due to sign-out or policy), so it's only really
    // meaningful when it's the *only* disable reason.
    // TODO(crbug.com/839834): Only set this reason when it's meaningful.
    DISABLE_REASON_USER_CHOICE = 1 << 3,
    // Sync has encountered an unrecoverable error. It won't attempt to start
    // again until either the browser is restarted, or the user fully signs out
    // and back in again.
    DISABLE_REASON_UNRECOVERABLE_ERROR = 1 << 4
  };

  // The overall state of the SyncService, in ascending order of "activeness".
  enum class State {
    // Sync is disabled, e.g. due to enterprise policy, or because the user has
    // disabled it, or simply because there is no authenticated user. Call
    // GetDisableReasons to figure out which of these it is.
    DISABLED,
    // Sync can start in principle, but nothing has prodded it to actually do it
    // yet. Note that during subsequent browser startups, Sync starts
    // automatically, i.e. no prod is necessary, but during the first start Sync
    // does need a kick. This usually happens via starting (not finishing!) the
    // initial setup, or via an explicit call to RequestStart.
    // TODO(crbug.com/839834): Check whether this state is necessary, or if Sync
    // can just always start up if all conditions are fulfilled (that's what
    // happens in practice anyway).
    WAITING_FOR_START_REQUEST,
    // Sync's startup was deferred, so that it doesn't slow down browser
    // startup. Once the deferral time (usually 10s) expires, or something
    // requests immediate startup, Sync will actually start.
    START_DEFERRED,
    // The Sync engine is in the process of initializing.
    INITIALIZING,
    // The Sync engine is initialized, but the process of configuring the data
    // types hasn't been started yet. This usually occurs if the user hasn't
    // completed the initial Sync setup yet (i.e. IsFirstSetupComplete() is
    // false), but it can also occur if a (non-initial) Sync setup happens to be
    // ongoing while the Sync service is starting up.
    PENDING_DESIRED_CONFIGURATION,
    // The Sync engine itself is up and running, but the individual data types
    // are being (re)configured. GetActiveDataTypes() will still be empty.
    CONFIGURING,
    // The Sync service is up and running. Note that this *still* doesn't
    // necessarily mean any particular data is being uploaded, e.g. individual
    // data types might be disabled or might have failed to start (check
    // GetActiveDataTypes()).
    ACTIVE
  };

  // Used to specify the kind of passphrase with which sync data is encrypted.
  enum PassphraseType {
    IMPLICIT,  // The user did not provide a custom passphrase for encryption.
               // We implicitly use the GAIA password in such cases.
    EXPLICIT,  // The user selected the "use custom passphrase" radio button
               // during sync setup and provided a passphrase.
  };

  // Passed as an argument to RequestStop to control whether or not the sync
  // engine should clear its data directory when it shuts down. See
  // RequestStop for more information.
  enum SyncStopDataFate {
    KEEP_DATA,
    CLEAR_DATA,
  };

  ~SyncService() override {}

  // Returns the set of reasons that are keeping Sync disabled, as a bitmask of
  // DisableReason enum entries.
  virtual int GetDisableReasons() const = 0;
  // Helper that returns whether GetDisableReasons() contains the given |reason|
  // (possibly among others).
  bool HasDisableReason(DisableReason reason) const {
    return GetDisableReasons() & reason;
  }

  // Returns the overall state of the SyncService. See the enum definition for
  // what the individual states mean.
  // Note: If your question is "Are we actually sending this data to Google?" or
  // "Am I allowed to send this type of data to Google?", you probably want
  // syncer::GetUploadToGoogleState instead of this.
  virtual State GetState() const = 0;

  // Whether the user has completed the initial Sync setup. This does not mean
  // that sync is currently running (due to delayed startup, unrecoverable
  // errors, or shutdown). If you want to know whether Sync is actually running,
  // use GetState instead.
  virtual bool IsFirstSetupComplete() const = 0;

  // DEPRECATED! Use GetDisableReasons/HasDisableReason instead.
  // Equivalent to "!HasDisableReason(DISABLE_REASON_PLATFORM_OVERRIDE) &&
  // !HasDisableReason(DISABLE_REASON_ENTERPRISE_POLICY)".
  bool IsSyncAllowed() const;

  // DEPRECATED! Use GetState instead. Equivalent to
  // "GetState() == State::CONFIGURING || GetState() == State::ACTIVE".
  // To see which datatypes are actually syncing, see GetActiveDataTypes().
  bool IsSyncActive() const;

  // Returns true if the local sync backend server has been enabled through a
  // command line flag or policy. In this case sync is considered active but any
  // implied consent for further related services e.g. Suggestions, Web History
  // etc. is considered not granted.
  virtual bool IsLocalSyncEnabled() const = 0;

  // Triggers a GetUpdates call for the specified |types|, pulling any new data
  // from the sync server.
  virtual void TriggerRefresh(const ModelTypeSet& types) = 0;

  // Get the set of current active data types (those chosen or configured by
  // the user which have not also encountered a runtime error).
  // Note that if the Sync engine is in the middle of a configuration, this
  // will the the empty set. Once the configuration completes the set will
  // be updated.
  virtual ModelTypeSet GetActiveDataTypes() const = 0;

  // Returns the SyncClient instance associated with this service.
  virtual SyncClient* GetSyncClient() const = 0;

  // Adds/removes an observer. SyncService does not take ownership of the
  // observer.
  virtual void AddObserver(SyncServiceObserver* observer) = 0;
  virtual void RemoveObserver(SyncServiceObserver* observer) = 0;

  // Returns true if |observer| has already been added as an observer.
  virtual bool HasObserver(const SyncServiceObserver* observer) const = 0;

  // ---------------------------------------------------------------------------
  // TODO(sync): The methods below were pulled from ProfileSyncService, and
  // should be evaluated to see if they should stay.

  // Called when a datatype (SyncableService) has a need for sync to start
  // ASAP, presumably because a local change event has occurred but we're
  // still in deferred start mode, meaning the SyncableService hasn't been
  // told to MergeDataAndStartSyncing yet.
  virtual void OnDataTypeRequestsSyncStartup(ModelType type) = 0;

  // DEPRECATED! Use GetDisableReasons/HasDisableReason instead.
  // Equivalent to having no disable reasons, i.e.
  // "GetDisableReasons() == DISABLE_REASON_NONE".
  bool CanSyncStart() const;

  // Stops sync at the user's request. |data_fate| controls whether the sync
  // engine should clear its data directory when it shuts down. Generally
  // KEEP_DATA is used when the user just stops sync, and CLEAR_DATA is used
  // when they sign out of the profile entirely.
  virtual void RequestStop(SyncStopDataFate data_fate) = 0;

  // The user requests that sync start. This only actually starts sync if
  // IsSyncAllowed is true and the user is signed in. Once sync starts,
  // other things such as IsFirstSetupComplete being false can still prevent
  // it from moving into the "active" state.
  virtual void RequestStart() = 0;

  // Returns the set of types which are preferred for enabling. This is a
  // superset of the active types (see GetActiveDataTypes()).
  virtual ModelTypeSet GetPreferredDataTypes() const = 0;

  // Called when a user chooses which data types to sync. |sync_everything|
  // represents whether they chose the "keep everything synced" option; if
  // true, |chosen_types| will be ignored and all data types will be synced.
  // |sync_everything| means "sync all current and future data types."
  // |chosen_types| must be a subset of UserSelectableTypes().
  virtual void OnUserChoseDatatypes(bool sync_everything,
                                    ModelTypeSet chosen_types) = 0;

  // Called whe Sync has been setup by the user and can be started.
  virtual void SetFirstSetupComplete() = 0;

  // Returns true if initial sync setup is in progress (does not return true
  // if the user is customizing sync after already completing setup once).
  // SyncService uses this to determine if it's OK to start syncing, or if the
  // user is still setting up the initial sync configuration.
  virtual bool IsFirstSetupInProgress() const = 0;

  // Called by the UI to notify the SyncService that UI is visible so it will
  // not start syncing. This tells sync whether it's safe to start downloading
  // data types yet (we don't start syncing until after sync setup is complete).
  // The UI calls this and holds onto the instance for as long as any part of
  // the signin wizard is displayed (even just the login UI).
  // When the last outstanding handle is deleted, this kicks off the sync engine
  // to ensure that data download starts. In this case,
  // |ReconfigureDatatypeManager| will get triggered.
  virtual std::unique_ptr<SyncSetupInProgressHandle>
  GetSetupInProgressHandle() = 0;

  // Used by tests.
  virtual bool IsSetupInProgress() const = 0;

  virtual const GoogleServiceAuthError& GetAuthError() const = 0;

  // DEPRECATED! Use GetDisableReasons/HasDisableReason instead.
  // Equivalent to "HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR)".
  bool HasUnrecoverableError() const;

  // Returns true if the SyncEngine has told us it's ready to accept changes.
  // DEPRECATED! Use GetState instead.
  virtual bool IsEngineInitialized() const = 0;

  // Return the active OpenTabsUIDelegate. If open/proxy tabs is not enabled or
  // not currently syncing, returns nullptr.
  virtual sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() = 0;

  // Returns true if OnPassphraseRequired has been called for decryption and
  // we have an encrypted data type enabled.
  virtual bool IsPassphraseRequiredForDecryption() const = 0;

  // Returns the time the current explicit passphrase (if any), was set.
  // If no secondary passphrase is in use, or no time is available, returns an
  // unset base::Time.
  virtual base::Time GetExplicitPassphraseTime() const = 0;

  // Returns true if a secondary (explicit) passphrase is being used. It is not
  // legal to call this method before the engine is initialized.
  virtual bool IsUsingSecondaryPassphrase() const = 0;

  // Turns on encryption for all data. Callers must call OnUserChoseDatatypes()
  // after calling this to force the encryption to occur.
  virtual void EnableEncryptEverything() = 0;

  // Returns true if we are currently set to encrypt all the sync data.
  virtual bool IsEncryptEverythingEnabled() const = 0;

  // Asynchronously sets the passphrase to |passphrase| for encryption. |type|
  // specifies whether the passphrase is a custom passphrase or the GAIA
  // password being reused as a passphrase.
  // TODO(atwilson): Change this so external callers can only set an EXPLICIT
  // passphrase with this API.
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       PassphraseType type) = 0;

  // Asynchronously decrypts pending keys using |passphrase|. Returns false
  // immediately if the passphrase could not be used to decrypt a locally cached
  // copy of encrypted keys; returns true otherwise.
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT = 0;

  // Checks whether the Cryptographer is ready to encrypt and decrypt updates
  // for sensitive data types. Caller must be holding a syncer::BaseTransaction
  // to ensure thread safety.
  virtual bool IsCryptographerReady(const BaseTransaction* trans) const = 0;

  // TODO(akalin): This is called mostly by ModelAssociators and
  // tests.  Figure out how to pass the handle to the ModelAssociators
  // directly, figure out how to expose this to tests, and remove this
  // function.
  virtual UserShare* GetUserShare() const = 0;

  // Returns DeviceInfo provider for the local device.
  virtual const LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() const = 0;

  // Called to re-enable a type disabled by DisableDatatype(..). Note, this does
  // not change the preferred state of a datatype, and is not persisted across
  // restarts.
  virtual void ReenableDatatype(ModelType type) = 0;

  // Return sync token status.
  virtual SyncTokenStatus GetSyncTokenStatus() const = 0;

  // Initializes a struct of status indicators with data from the engine.
  // Returns false if the engine was not available for querying; in that case
  // the struct will be filled with default data.
  virtual bool QueryDetailedSyncStatus(SyncStatus* result) = 0;

  // Returns the last synced time.
  virtual base::Time GetLastSyncedTime() const = 0;

  virtual SyncCycleSnapshot GetLastCycleSnapshot() const = 0;

  // Returns a ListValue indicating the status of all registered types.
  //
  // The format is:
  // [ {"name": <name>, "value": <value>, "status": <status> }, ... ]
  // where <name> is a type's name, <value> is a string providing details for
  // the type's status, and <status> is one of "error", "warning" or "ok"
  // depending on the type's current status.
  //
  // This function is used by about_sync_util.cc to help populate the about:sync
  // page.  It returns a ListValue rather than a DictionaryValue in part to make
  // it easier to iterate over its elements when constructing that page.
  virtual std::unique_ptr<base::Value> GetTypeStatusMap() = 0;

  virtual const GURL& sync_service_url() const = 0;

  virtual std::string unrecoverable_error_message() const = 0;
  virtual base::Location unrecoverable_error_location() const = 0;

  virtual void AddProtocolEventObserver(ProtocolEventObserver* observer) = 0;
  virtual void RemoveProtocolEventObserver(ProtocolEventObserver* observer) = 0;

  virtual void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) = 0;
  virtual void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) = 0;

  // Returns a weak pointer to the service's JsController.
  virtual base::WeakPtr<JsController> GetJsController() = 0;

  // Asynchronously fetches base::Value representations of all sync nodes and
  // returns them to the specified callback on this thread.
  //
  // These requests can live a long time and return when you least expect it.
  // For safety, the callback should be bound to some sort of WeakPtr<> or
  // scoped_refptr<>.
  virtual void GetAllNodes(
      const base::Callback<void(std::unique_ptr<base::ListValue>)>&
          callback) = 0;

  // Information about the currently signed in user.
  virtual AccountInfo GetAuthenticatedAccountInfo() const = 0;

  virtual GlobalIdMapper* GetGlobalIdMapper() const = 0;

 protected:
  SyncService() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_H_
