// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_IMPL_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/extensions_activity_monitor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "components/invalidation/invalidation_handler.h"
#include "components/sync_driver/backend_data_type_configurer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/report_unrecoverable_error_function.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/util/extensions_activity.h"

class GURL;
class Profile;

namespace base {
class MessageLoop;
}

namespace invalidation {
class InvalidationService;
}

namespace syncer {
class NetworkResources;
class SyncManagerFactory;
}

namespace sync_driver {
class SyncPrefs;
}

namespace browser_sync {

class ChangeProcessor;
class SyncBackendHostCore;
class SyncBackendRegistrar;
class SyncedDeviceTracker;
struct DoInitializeOptions;

// The only real implementation of the SyncBackendHost.  See that interface's
// definition for documentation of public methods.
class SyncBackendHostImpl
    : public SyncBackendHost,
      public content::NotificationObserver,
      public syncer::InvalidationHandler {
 public:
  typedef syncer::SyncStatus Status;

  // Create a SyncBackendHost with a reference to the |frontend| that
  // it serves and communicates to via the SyncFrontend interface (on
  // the same thread it used to call the constructor).  Must outlive
  // |sync_prefs|.
  SyncBackendHostImpl(const std::string& name,
                      Profile* profile,
                      invalidation::InvalidationService* invalidator,
                      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
                      const base::FilePath& sync_folder);
  virtual ~SyncBackendHostImpl();

  // SyncBackendHost implementation.
  virtual void Initialize(
      sync_driver::SyncFrontend* frontend,
      scoped_ptr<base::Thread> sync_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
      syncer::ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      syncer::NetworkResources* network_resources) OVERRIDE;
  virtual void UpdateCredentials(
      const syncer::SyncCredentials& credentials) OVERRIDE;
  virtual void StartSyncingWithServer() OVERRIDE;
  virtual void SetEncryptionPassphrase(
      const std::string& passphrase,
      bool is_explicit) OVERRIDE;
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      OVERRIDE WARN_UNUSED_RESULT;
  virtual void StopSyncingForShutdown() OVERRIDE;
  virtual scoped_ptr<base::Thread> Shutdown(syncer::ShutdownReason reason)
      OVERRIDE;
  virtual void UnregisterInvalidationIds() OVERRIDE;
  virtual void ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) OVERRIDE;
  virtual void ActivateDataType(
     syncer::ModelType type, syncer::ModelSafeGroup group,
     sync_driver::ChangeProcessor* change_processor) OVERRIDE;
  virtual void DeactivateDataType(syncer::ModelType type) OVERRIDE;
  virtual void EnableEncryptEverything() OVERRIDE;
  virtual syncer::UserShare* GetUserShare() const OVERRIDE;
  virtual scoped_ptr<syncer::SyncContextProxy> GetSyncContextProxy() OVERRIDE;
  virtual Status GetDetailedStatus() OVERRIDE;
  virtual syncer::sessions::SyncSessionSnapshot
      GetLastSessionSnapshot() const OVERRIDE;
  virtual bool HasUnsyncedItems() const OVERRIDE;
  virtual bool IsNigoriEnabled() const OVERRIDE;
  virtual syncer::PassphraseType GetPassphraseType() const OVERRIDE;
  virtual base::Time GetExplicitPassphraseTime() const OVERRIDE;
  virtual bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const OVERRIDE;
  virtual void GetModelSafeRoutingInfo(
      syncer::ModelSafeRoutingInfo* out) const OVERRIDE;
  virtual SyncedDeviceTracker* GetSyncedDeviceTracker() const OVERRIDE;
  virtual void RequestBufferedProtocolEventsAndEnableForwarding() OVERRIDE;
  virtual void DisableProtocolEventForwarding() OVERRIDE;
  virtual void EnableDirectoryTypeDebugInfoForwarding() OVERRIDE;
  virtual void DisableDirectoryTypeDebugInfoForwarding() OVERRIDE;
  virtual void GetAllNodesForTypes(
      syncer::ModelTypeSet types,
      base::Callback<void(const std::vector<syncer::ModelType>&,
                          ScopedVector<base::ListValue>)> type) OVERRIDE;
  virtual base::MessageLoop* GetSyncLoopForTesting() OVERRIDE;

 protected:
  // The types and functions below are protected so that test
  // subclasses can use them.

  // Allows tests to perform alternate core initialization work.
  virtual void InitCore(scoped_ptr<DoInitializeOptions> options);

  // Request the syncer to reconfigure with the specfied params.
  // Virtual for testing.
  virtual void RequestConfigureSyncer(
      syncer::ConfigureReason reason,
      syncer::ModelTypeSet to_download,
      syncer::ModelTypeSet to_purge,
      syncer::ModelTypeSet to_journal,
      syncer::ModelTypeSet to_unapply,
      syncer::ModelTypeSet to_ignore,
      const syncer::ModelSafeRoutingInfo& routing_info,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task,
      const base::Closure& retry_callback);

  // Called when the syncer has finished performing a configuration.
  void FinishConfigureDataTypesOnFrontendLoop(
      const syncer::ModelTypeSet enabled_types,
      const syncer::ModelTypeSet succeeded_configuration_types,
      const syncer::ModelTypeSet failed_configuration_types,
      const base::Callback<void(syncer::ModelTypeSet,
                                syncer::ModelTypeSet)>& ready_task);

  // Reports backend initialization success.  Includes some objects from sync
  // manager initialization to be passed back to the UI thread.
  //
  // |sync_context_proxy| points to an object owned by the SyncManager.
  // Ownership is not transferred, but we can obtain our own copy of the object
  // using its Clone() method.
  virtual void HandleInitializationSuccessOnFrontendLoop(
      const syncer::WeakHandle<syncer::JsBackend> js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
          debug_info_listener,
      syncer::SyncContextProxy* sync_context_proxy,
      const std::string& cache_guid);

  // Downloading of control types failed and will be retried. Invokes the
  // frontend's sync configure retry method.
  void HandleControlTypesDownloadRetry();

  // Forwards a ProtocolEvent to the frontend.  Will not be called unless a
  // call to SetForwardProtocolEvents() explicitly requested that we start
  // forwarding these events.
  void HandleProtocolEventOnFrontendLoop(syncer::ProtocolEvent* event);

  // Forwards a directory commit counter update to the frontend loop.  Will not
  // be called unless a call to EnableDirectoryTypeDebugInfoForwarding()
  // explicitly requested that we start forwarding these events.
  void HandleDirectoryCommitCountersUpdatedOnFrontendLoop(
      syncer::ModelType type,
      const syncer::CommitCounters& counters);

  // Forwards a directory update counter update to the frontend loop.  Will not
  // be called unless a call to EnableDirectoryTypeDebugInfoForwarding()
  // explicitly requested that we start forwarding these events.
  void HandleDirectoryUpdateCountersUpdatedOnFrontendLoop(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters);

  // Forwards a directory status counter update to the frontend loop.  Will not
  // be called unless a call to EnableDirectoryTypeDebugInfoForwarding()
  // explicitly requested that we start forwarding these events.
  void HandleDirectoryStatusCountersUpdatedOnFrontendLoop(
      syncer::ModelType type,
      const syncer::StatusCounters& counters);

  sync_driver::SyncFrontend* frontend() { return frontend_; }

 private:
  friend class SyncBackendHostCore;

  // Checks if we have received a notice to turn on experimental datatypes
  // (via the nigori node) and informs the frontend if that is the case.
  // Note: it is illegal to call this before the backend is initialized.
  void AddExperimentalTypes();

  // Handles backend initialization failure.
  void HandleInitializationFailureOnFrontendLoop();

  // Called from Core::OnSyncCycleCompleted to handle updating frontend
  // thread components.
  void HandleSyncCycleCompletedOnFrontendLoop(
      const syncer::sessions::SyncSessionSnapshot& snapshot);

  // Called when the syncer failed to perform a configuration and will
  // eventually retry. FinishingConfigurationOnFrontendLoop(..) will be called
  // on successful completion.
  void RetryConfigurationOnFrontendLoop(const base::Closure& retry_callback);

  // Helpers to persist a token that can be used to bootstrap sync encryption
  // across browser restart to avoid requiring the user to re-enter their
  // passphrase.  |token| must be valid UTF-8 as we use the PrefService for
  // storage.
  void PersistEncryptionBootstrapToken(
      const std::string& token,
      syncer::BootstrapTokenType token_type);

  // For convenience, checks if initialization state is INITIALIZED.
  bool initialized() const { return initialized_; }

  // Let the front end handle the actionable error event.
  void HandleActionableErrorEventOnFrontendLoop(
      const syncer::SyncProtocolError& sync_error);

  // Handle a migration request.
  void HandleMigrationRequestedOnFrontendLoop(const syncer::ModelTypeSet types);

  // Checks if |passphrase| can be used to decrypt the cryptographer's pending
  // keys that were cached during NotifyPassphraseRequired. Returns true if
  // decryption was successful. Returns false otherwise. Must be called with a
  // non-empty pending keys cache.
  bool CheckPassphraseAgainstCachedPendingKeys(
      const std::string& passphrase) const;

  // Invoked when a passphrase is required to decrypt a set of Nigori keys,
  // or for encrypting. |reason| denotes why the passphrase was required.
  // |pending_keys| is a copy of the cryptographer's pending keys, that are
  // cached by the frontend. If there are no pending keys, or if the passphrase
  // required reason is REASON_ENCRYPTION, an empty EncryptedData object is
  // passed.
  void NotifyPassphraseRequired(syncer::PassphraseRequiredReason reason,
                                sync_pb::EncryptedData pending_keys);

  // Invoked when the passphrase provided by the user has been accepted.
  void NotifyPassphraseAccepted();

  // Invoked when the set of encrypted types or the encrypt
  // everything flag changes.
  void NotifyEncryptedTypesChanged(
      syncer::ModelTypeSet encrypted_types,
      bool encrypt_everything);

  // Invoked when sync finishes encrypting new datatypes.
  void NotifyEncryptionComplete();

  // Invoked when the passphrase state has changed. Caches the passphrase state
  // for later use on the UI thread.
  // If |type| is FROZEN_IMPLICIT_PASSPHRASE or CUSTOM_PASSPHRASE,
  // |explicit_passphrase_time| is the time at which that passphrase was set
  // (if available).
  void HandlePassphraseTypeChangedOnFrontendLoop(
      syncer::PassphraseType type,
      base::Time explicit_passphrase_time);

  void HandleStopSyncingPermanentlyOnFrontendLoop();

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(
      syncer::ConnectionStatus status);

  // NotificationObserver implementation.
  virtual void Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) OVERRIDE;

  // InvalidationHandler implementation.
  virtual void OnInvalidatorStateChange(
      syncer::InvalidatorState state) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE;
  virtual std::string GetOwnerName() const OVERRIDE;

  content::NotificationRegistrar notification_registrar_;

  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  base::MessageLoop* const frontend_loop_;

  Profile* const profile_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  // Our core, which communicates directly to the syncapi. Use refptr instead
  // of WeakHandle because |core_| is created on UI loop but released on
  // sync loop.
  scoped_refptr<SyncBackendHostCore> core_;

  // A handle referencing the main interface for non-blocking sync types.
  scoped_ptr<syncer::SyncContextProxy> sync_context_proxy_;

  bool initialized_;

  const base::WeakPtr<sync_driver::SyncPrefs> sync_prefs_;

  ExtensionsActivityMonitor extensions_activity_monitor_;

  scoped_ptr<SyncBackendRegistrar> registrar_;

  // The frontend which we serve (and are owned by).
  sync_driver::SyncFrontend* frontend_;

  // We cache the cryptographer's pending keys whenever NotifyPassphraseRequired
  // is called. This way, before the UI calls SetDecryptionPassphrase on the
  // syncer, it can avoid the overhead of an asynchronous decryption call and
  // give the user immediate feedback about the passphrase entered by first
  // trying to decrypt the cached pending keys on the UI thread. Note that
  // SetDecryptionPassphrase can still fail after the cached pending keys are
  // successfully decrypted if the pending keys have changed since the time they
  // were cached.
  sync_pb::EncryptedData cached_pending_keys_;

  // The state of the passphrase required to decrypt the bag of encryption keys
  // in the nigori node. Updated whenever a new nigori node arrives or the user
  // manually changes their passphrase state. Cached so we can synchronously
  // check it from the UI thread.
  syncer::PassphraseType cached_passphrase_type_;

  // If an explicit passphrase is in use, the time at which the passphrase was
  // first set (if available).
  base::Time cached_explicit_passphrase_time_;

  // UI-thread cache of the last SyncSessionSnapshot received from syncapi.
  syncer::sessions::SyncSessionSnapshot last_snapshot_;

  invalidation::InvalidationService* invalidator_;
  bool invalidation_handler_registered_;

  base::WeakPtrFactory<SyncBackendHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHostImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_IMPL_H_

