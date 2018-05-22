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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace syncer {

class ChangeProcessor;
class SyncBackendHostCore;
class SyncBackendRegistrar;
class SyncClient;
class SyncPrefs;

// The only real implementation of the SyncEngine. See that interface's
// definition for documentation of public methods.
class SyncBackendHostImpl : public SyncEngine, public InvalidationHandler {
 public:
  using Status = SyncStatus;

  SyncBackendHostImpl(const std::string& name,
                      SyncClient* sync_client,
                      invalidation::InvalidationService* invalidator,
                      const base::WeakPtr<SyncPrefs>& sync_prefs,
                      const base::FilePath& sync_data_folder);
  ~SyncBackendHostImpl() override;

  // SyncEngine implementation.
  void Initialize(InitParams params) override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void UpdateCredentials(const SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartConfiguration() override;
  void StartSyncingWithServer() override;
  void SetEncryptionPassphrase(const std::string& passphrase,
                               bool is_explicit) override;
  void SetDecryptionPassphrase(const std::string& passphrase) override;
  void StopSyncingForShutdown() override;
  void Shutdown(ShutdownReason reason) override;
  void ConfigureDataTypes(ConfigureParams params) override;
  void RegisterDirectoryDataType(ModelType type, ModelSafeGroup group) override;
  void UnregisterDirectoryDataType(ModelType type) override;
  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override;
  void DeactivateDirectoryDataType(ModelType type) override;
  void ActivateNonBlockingDataType(ModelType type,
                                   std::unique_ptr<ActivationContext>) override;
  void DeactivateNonBlockingDataType(ModelType type) override;
  void EnableEncryptEverything() override;
  UserShare* GetUserShare() const override;
  Status GetDetailedStatus() override;
  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;
  bool IsCryptographerReady(const BaseTransaction* trans) const override;
  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const override;
  void FlushDirectory() const override;
  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;
  void EnableDirectoryTypeDebugInfoForwarding() override;
  void DisableDirectoryTypeDebugInfoForwarding() override;
  void ClearServerData(const base::Closure& callback) override;
  void OnCookieJarChanged(bool account_mismatch,
                          bool empty_jar,
                          const base::Closure& callback) override;

  // InvalidationHandler implementation.
  void OnInvalidatorStateChange(InvalidatorState state) override;
  void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) override;
  std::string GetOwnerName() const override;

 protected:
  // The types and functions below are protected so that test
  // subclasses can use them.

  // Called when the syncer has finished performing a configuration.
  void FinishConfigureDataTypesOnFrontendLoop(
      const ModelTypeSet enabled_types,
      const ModelTypeSet succeeded_configuration_types,
      const ModelTypeSet failed_configuration_types,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task);

  // Reports backend initialization success.  Includes some objects from sync
  // manager initialization to be passed back to the UI thread.
  //
  // |model_type_connector| is our ModelTypeConnector, which is owned because in
  // production it is a proxy object to the real ModelTypeConnector.
  virtual void HandleInitializationSuccessOnFrontendLoop(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend> js_backend,
      const WeakHandle<DataTypeDebugInfoListener> debug_info_listener,
      std::unique_ptr<ModelTypeConnector> model_type_connector,
      const std::string& cache_guid);

  // Forwards a ProtocolEvent to the host. Will not be called unless a call to
  // SetForwardProtocolEvents() explicitly requested that we start forwarding
  // these events.
  void HandleProtocolEventOnFrontendLoop(std::unique_ptr<ProtocolEvent> event);

  // Forwards a directory commit counter update to the frontend loop.  Will not
  // be called unless a call to EnableDirectoryTypeDebugInfoForwarding()
  // explicitly requested that we start forwarding these events.
  void HandleDirectoryCommitCountersUpdatedOnFrontendLoop(
      ModelType type,
      const CommitCounters& counters);

  // Forwards a directory update counter update to the frontend loop.  Will not
  // be called unless a call to EnableDirectoryTypeDebugInfoForwarding()
  // explicitly requested that we start forwarding these events.
  void HandleDirectoryUpdateCountersUpdatedOnFrontendLoop(
      ModelType type,
      const UpdateCounters& counters);

  // Forwards a directory status counter update to the frontend loop.  Will not
  // be called unless a call to EnableDirectoryTypeDebugInfoForwarding()
  // explicitly requested that we start forwarding these events.
  void HandleDirectoryStatusCountersUpdatedOnFrontendLoop(
      ModelType type,
      const StatusCounters& counters);

  // Overwrites the kSyncInvalidationVersions preference with the most recent
  // set of invalidation versions for each type.
  void UpdateInvalidationVersions(
      const std::map<ModelType, int64_t>& invalidation_versions);

  SyncEngineHost* host() { return host_; }

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
      const SyncCycleSnapshot& snapshot);

  // Called when the syncer failed to perform a configuration and will
  // eventually retry. FinishingConfigurationOnFrontendLoop(..) will be called
  // on successful completion.
  void RetryConfigurationOnFrontendLoop(const base::Closure& retry_callback);

  // For convenience, checks if initialization state is INITIALIZED.
  bool initialized() const { return initialized_; }

  // Let the front end handle the actionable error event.
  void HandleActionableErrorEventOnFrontendLoop(
      const SyncProtocolError& sync_error);

  // Handle a migration request.
  void HandleMigrationRequestedOnFrontendLoop(const ModelTypeSet types);

  // Dispatched to from OnConnectionStatusChange to handle updating
  // frontend UI components.
  void HandleConnectionStatusChangeOnFrontendLoop(ConnectionStatus status);

  void ClearServerDataDoneOnFrontendLoop(
      const base::Closure& frontend_callback);

  void OnCookieJarChangedDoneOnFrontendLoop(const base::Closure& callback);

  SyncClient* const sync_client_;

  // The task runner where all the sync engine operations happen.
  scoped_refptr<base::SingleThreadTaskRunner> sync_task_runner_;

  // Name used for debugging (set from profile_->GetDebugName()).
  const std::string name_;

  // Our core, which communicates directly to the syncapi. Use refptr instead
  // of WeakHandle because |core_| is created on UI loop but released on
  // sync loop.
  scoped_refptr<SyncBackendHostCore> core_;

  // A handle referencing the main interface for non-blocking sync types. This
  // object is owned because in production code it is a proxy object.
  std::unique_ptr<ModelTypeConnector> model_type_connector_;

  bool initialized_ = false;

  const base::WeakPtr<SyncPrefs> sync_prefs_;

  // The host which we serve (and are owned by). Set in Initialize() and nulled
  // out in StopSyncingForShutdown().
  SyncEngineHost* host_ = nullptr;

  // A pointer to the registrar; owned by |core_|.
  SyncBackendRegistrar* registrar_ = nullptr;

  invalidation::InvalidationService* invalidator_;
  bool invalidation_handler_registered_ = false;

  // Checks that we're on the same thread this was constructed on (UI thread).
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SyncBackendHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHostImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_IMPL_H_
