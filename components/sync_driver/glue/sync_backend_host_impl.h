// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_IMPL_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/sync_driver/backend_data_type_configurer.h"
#include "components/sync_driver/glue/sync_backend_host.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/internal_api/public/sync_manager.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/protocol/encryption.pb.h"
#include "sync/protocol/sync_protocol_error.h"
#include "sync/util/extensions_activity.h"

class GURL;

namespace base {
class MessageLoop;
}

namespace invalidation {
class InvalidationService;
}

namespace syncer {
class SyncManagerFactory;
class UnrecoverableErrorHandler;
}

namespace sync_driver {
class SyncClient;
class SyncPrefs;
}

namespace browser_sync {

class ChangeProcessor;
class SyncBackendHostCore;
class SyncBackendRegistrar;
struct DoInitializeOptions;

// The only real implementation of the SyncBackendHost.  See that interface's
// definition for documentation of public methods.
class SyncBackendHostImpl
    : public SyncBackendHost,
      public syncer::InvalidationHandler {
 public:
  typedef syncer::SyncStatus Status;

  // Create a SyncBackendHost with a reference to the |frontend| that
  // it serves and communicates to via the SyncFrontend interface (on
  // the same thread it used to call the constructor).  Must outlive
  // |sync_prefs|.
  SyncBackendHostImpl(
      const std::string& name,
      sync_driver::SyncClient* sync_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder);
  ~SyncBackendHostImpl() override;

  // SyncBackendHost implementation.
  void Initialize(
      sync_driver::SyncFrontend* frontend,
      std::unique_ptr<base::Thread> sync_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      std::unique_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>&
          unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      const HttpPostProviderFactoryGetter& http_post_provider_factory_getter,
      std::unique_ptr<syncer::SyncEncryptionHandler::NigoriState>
          saved_nigori_state) override;
  void TriggerRefresh(const syncer::ModelTypeSet& types) override;
  void UpdateCredentials(const syncer::SyncCredentials& credentials) override;
  void StartSyncingWithServer() override;
  void SetEncryptionPassphrase(const std::string& passphrase,
                               bool is_explicit) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override
      WARN_UNUSED_RESULT;
  void StopSyncingForShutdown() override;
  std::unique_ptr<base::Thread> Shutdown(
      syncer::ShutdownReason reason) override;
  void UnregisterInvalidationIds() override;
  syncer::ModelTypeSet ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
          ready_task,
      const base::Callback<void()>& retry_callback) override;
  void ActivateDirectoryDataType(
      syncer::ModelType type,
      syncer::ModelSafeGroup group,
      sync_driver::ChangeProcessor* change_processor) override;
  void DeactivateDirectoryDataType(syncer::ModelType type) override;
  void ActivateNonBlockingDataType(
      syncer::ModelType type,
      std::unique_ptr<syncer_v2::ActivationContext>) override;
  void DeactivateNonBlockingDataType(syncer::ModelType type) override;
  void EnableEncryptEverything() override;
  syncer::UserShare* GetUserShare() const override;
  Status GetDetailedStatus() override;
  syncer::sessions::SyncSessionSnapshot GetLastSessionSnapshot() const override;
  bool HasUnsyncedItems() const override;
  bool IsNigoriEnabled() const override;
  syncer::PassphraseType GetPassphraseType() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const override;
  void GetModelSafeRoutingInfo(
      syncer::ModelSafeRoutingInfo* out) const override;
  void FlushDirectory() const override;
  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;
  void EnableDirectoryTypeDebugInfoForwarding() override;
  void DisableDirectoryTypeDebugInfoForwarding() override;
  void GetAllNodesForTypes(
      syncer::ModelTypeSet types,
      base::Callback<void(const std::vector<syncer::ModelType>&,
                          ScopedVector<base::ListValue>)> type) override;
  base::MessageLoop* GetSyncLoopForTesting() override;
  void RefreshTypesForTest(syncer::ModelTypeSet types) override;
  void ClearServerData(
      const syncer::SyncManager::ClearServerDataCallback& callback) override;
  void OnCookieJarChanged(bool account_mismatch) override;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 protected:
  // The types and functions below are protected so that test
  // subclasses can use them.

  // Allows tests to perform alternate core initialization work.
  virtual void InitCore(std::unique_ptr<DoInitializeOptions> options);

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
  // |model_type_connector| is our ModelTypeConnector, which is owned because in
  // production it is a proxy object to the real ModelTypeConnector.
  virtual void HandleInitializationSuccessOnFrontendLoop(
      const syncer::WeakHandle<syncer::JsBackend> js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
          debug_info_listener,
      std::unique_ptr<syncer_v2::ModelTypeConnector> model_type_connector,
      const std::string& cache_guid);

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

  // Overwrites the kSyncInvalidationVersions preference with the most recent
  // set of invalidation versions for each type.
  void UpdateInvalidationVersions(
      const std::map<syncer::ModelType, int64_t>& invalidation_versions);

  sync_driver::SyncFrontend* frontend() {
    return frontend_;
  }

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

  void HandleLocalSetPassphraseEncryptionOnFrontendLoop(
      const syncer::SyncEncryptionHandler::NigoriState& nigori_state);

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(
      syncer::ConnectionStatus status);

  void ClearServerDataDoneOnFrontendLoop(
      const syncer::SyncManager::ClearServerDataCallback& frontend_callback);

  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  base::MessageLoop* const frontend_loop_;

  sync_driver::SyncClient* const sync_client_;

  // The UI thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  // Our core, which communicates directly to the syncapi. Use refptr instead
  // of WeakHandle because |core_| is created on UI loop but released on
  // sync loop.
  scoped_refptr<SyncBackendHostCore> core_;

  // A handle referencing the main interface for non-blocking sync types. This
  // object is owned because in production code it is a proxy object.
  std::unique_ptr<syncer_v2::ModelTypeConnector> model_type_connector_;

  bool initialized_;

  const base::WeakPtr<sync_driver::SyncPrefs> sync_prefs_;

  std::unique_ptr<SyncBackendRegistrar> registrar_;

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

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_IMPL_H_
