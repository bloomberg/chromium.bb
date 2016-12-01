// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/backend_data_type_configurer.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/shutdown_reason.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_manager_factory.h"

class GURL;

namespace syncer {

class CancelationSignal;
class HttpPostProviderFactory;
class SyncFrontend;
class SyncManagerFactory;
class UnrecoverableErrorHandler;

// An API to "host" the top level SyncAPI element.
//
// This class handles dispatch of potentially blocking calls to appropriate
// threads and ensures that the SyncFrontend is only accessed on the UI loop.
class SyncBackendHost : public BackendDataTypeConfigurer {
 public:
  typedef SyncStatus Status;
  typedef base::Callback<std::unique_ptr<HttpPostProviderFactory>(
      CancelationSignal*)>
      HttpPostProviderFactoryGetter;

  // Stubs used by implementing classes.
  SyncBackendHost();
  ~SyncBackendHost() override;

  // Called on the frontend's thread to kick off asynchronous initialization.
  // Optionally deletes the "Sync Data" folder during init in order to make
  // sure we're starting fresh.
  //
  // |saved_nigori_state| is optional nigori state to restore from a previous
  // backend instance. May be null.
  virtual void Initialize(
      SyncFrontend* frontend,
      scoped_refptr<base::SingleThreadTaskRunner> sync_task_runner,
      const WeakHandle<JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      const SyncCredentials& credentials,
      bool delete_sync_data_folder,
      bool enable_local_sync_backend,
      const base::FilePath& local_sync_backend_folder,
      std::unique_ptr<SyncManagerFactory> sync_manager_factory,
      const WeakHandle<UnrecoverableErrorHandler>& unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      const HttpPostProviderFactoryGetter& http_post_provider_factory_getter,
      std::unique_ptr<SyncEncryptionHandler::NigoriState>
          saved_nigori_state) = 0;

  // Called on the frontend's thread to trigger a refresh.
  virtual void TriggerRefresh(const ModelTypeSet& types) = 0;

  // Called on the frontend's thread to update SyncCredentials.
  virtual void UpdateCredentials(const SyncCredentials& credentials) = 0;

  // This starts the SyncerThread running a Syncer object to communicate with
  // sync servers.  Until this is called, no changes will leave or enter this
  // browser from the cloud / sync servers.
  // Called on |frontend_loop_|.
  virtual void StartSyncingWithServer() = 0;

  // Called on |frontend_loop_| to asynchronously set a new passphrase for
  // encryption. Note that it is an error to call SetEncryptionPassphrase under
  // the following circumstances:
  // - An explicit passphrase has already been set
  // - |is_explicit| is true and we have pending keys.
  // When |is_explicit| is false, a couple of things could happen:
  // - If there are pending keys, we try to decrypt them. If decryption works,
  //   this acts like a call to SetDecryptionPassphrase. If not, the GAIA
  //   passphrase passed in is cached so we can re-encrypt with it in future.
  // - If there are no pending keys, data is encrypted with |passphrase| (this
  //   is a no-op if data was already encrypted with |passphrase|.)
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       bool is_explicit) = 0;

  // Called on |frontend_loop_| to use the provided passphrase to asynchronously
  // attempt decryption. Returns false immediately if the passphrase could not
  // be used to decrypt a locally cached copy of encrypted keys; returns true
  // otherwise. If new encrypted keys arrive during the asynchronous call,
  // OnPassphraseRequired may be triggered at a later time. It is an error to
  // call this when there are no pending keys.
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT = 0;

  // Called on |frontend_loop_| to kick off shutdown procedure.  Attempts to cut
  // short any long-lived or blocking sync thread tasks so that the shutdown on
  // sync thread task that we're about to post won't have to wait very long.
  virtual void StopSyncingForShutdown() = 0;

  // Called on |frontend_loop_| to kick off shutdown.
  // See the implementation and Core::DoShutdown for details.
  // Must be called *after* StopSyncingForShutdown.
  virtual void Shutdown(ShutdownReason reason) = 0;

  // Removes all current registrations from the backend on the
  // InvalidationService.
  virtual void UnregisterInvalidationIds() = 0;

  // Changes the set of data types that are currently being synced.
  // The ready_task will be run when configuration is done with the
  // set of all types that failed configuration (i.e., if its argument
  // is non-empty, then an error was encountered).
  // Returns the set of types that are ready to start without needing any
  // further sync activity.
  // BackendDataTypeConfigurer implementation.
  ModelTypeSet ConfigureDataTypes(
      ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) override = 0;

  // Turns on encryption of all present and future sync data.
  virtual void EnableEncryptEverything() = 0;

  // Called on |frontend_loop_| to obtain a handle to the UserShare needed for
  // creating transactions.  Should not be called before we signal
  // initialization is complete with OnBackendInitialized().
  virtual UserShare* GetUserShare() const = 0;

  // Called from any thread to obtain current status information in detailed or
  // summarized form.
  virtual Status GetDetailedStatus() = 0;
  virtual SyncCycleSnapshot GetLastCycleSnapshot() const = 0;

  // Determines if the underlying sync engine has made any local changes to
  // items that have not yet been synced with the server.
  // ONLY CALL THIS IF OnInitializationComplete was called!
  virtual bool HasUnsyncedItems() const = 0;

  // Whether or not we are syncing encryption keys.
  virtual bool IsNigoriEnabled() const = 0;

  // Returns the type of passphrase being used to encrypt data. See
  // sync_encryption_handler.h.
  virtual PassphraseType GetPassphraseType() const = 0;

  // If an explicit passphrase is in use, returns the time at which that
  // passphrase was set (if available).
  virtual base::Time GetExplicitPassphraseTime() const = 0;

  // True if the cryptographer has any keys available to attempt decryption.
  // Could mean we've downloaded and loaded Nigori objects, or we bootstrapped
  // using a token previously received.
  virtual bool IsCryptographerReady(const BaseTransaction* trans) const = 0;

  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const = 0;

  // Send a message to the sync thread to persist the Directory to disk.
  virtual void FlushDirectory() const = 0;

  // Requests that the backend forward to the fronent any protocol events in
  // its buffer and begin forwarding automatically from now on.  Repeated calls
  // to this function may result in the same events being emitted several
  // times.
  virtual void RequestBufferedProtocolEventsAndEnableForwarding() = 0;

  // Disables protocol event forwarding.
  virtual void DisableProtocolEventForwarding() = 0;

  // Enables the sending of directory type debug counters.  Also, for every
  // time it is called, it makes an explicit request that updates to an update
  // for all counters be emitted.
  virtual void EnableDirectoryTypeDebugInfoForwarding() = 0;

  // Disables the sending of directory type debug counters.
  virtual void DisableDirectoryTypeDebugInfoForwarding() = 0;

  // Triggers sync cycle to update |types|.
  virtual void RefreshTypesForTest(ModelTypeSet types) = 0;

  // See SyncManager::ClearServerData.
  virtual void ClearServerData(
      const SyncManager::ClearServerDataCallback& callback) = 0;

  // Notify the syncer that the cookie jar has changed.
  // See SyncManager::OnCookieJarChanged.
  virtual void OnCookieJarChanged(bool account_mismatch, bool empty_jar) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncBackendHost);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_HOST_H_
