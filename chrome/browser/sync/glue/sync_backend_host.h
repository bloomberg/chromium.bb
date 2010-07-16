// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "base/timer.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/sync/notification_method.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"

class CancelableTask;
class Profile;

namespace browser_sync {

namespace sessions {
struct SyncSessionSnapshot;
}

class ChangeProcessor;
class DataTypeController;

// SyncFrontend is the interface used by SyncBackendHost to communicate with
// the entity that created it and, presumably, is interested in sync-related
// activity.
// NOTE: All methods will be invoked by a SyncBackendHost on the same thread
// used to create that SyncBackendHost.
class SyncFrontend {
 public:
  SyncFrontend() {}

  // The backend has completed initialization and it is now ready to accept and
  // process changes.
  virtual void OnBackendInitialized() = 0;

  // The backend queried the server recently and received some updates.
  virtual void OnSyncCycleCompleted() = 0;

  // The backend encountered an authentication problem and requests new
  // credentials to be provided. See SyncBackendHost::Authenticate for details.
  virtual void OnAuthError() = 0;

  // We are no longer permitted to communicate with the server. Sync should
  // be disabled and state cleaned up at once.
  virtual void OnStopSyncingPermanently() = 0;

 protected:
  // Don't delete through SyncFrontend interface.
  virtual ~SyncFrontend() {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SyncFrontend);
};

// A UI-thread safe API into the sync backend that "hosts" the top-level
// syncapi element, the SyncManager, on its own thread. This class handles
// dispatch of potentially blocking calls to appropriate threads and ensures
// that the SyncFrontend is only accessed on the UI loop.
class SyncBackendHost : public browser_sync::ModelSafeWorkerRegistrar {
 public:
  typedef sync_api::UserShare* UserShareHandle;
  typedef sync_api::SyncManager::Status::Summary StatusSummary;
  typedef sync_api::SyncManager::Status Status;
  typedef std::map<ModelSafeGroup,
                   scoped_refptr<browser_sync::ModelSafeWorker> > WorkerMap;

  // Create a SyncBackendHost with a reference to the |frontend| that it serves
  // and communicates to via the SyncFrontend interface (on the same thread
  // it used to call the constructor).
  SyncBackendHost(SyncFrontend* frontend,
                  Profile* profile,
                  const FilePath& profile_path,
                  const DataTypeController::TypeMap& data_type_controllers);
  // For testing.
  // TODO(skrul): Extract an interface so this is not needed.
  SyncBackendHost();
  ~SyncBackendHost();

  // Called on |frontend_loop_| to kick off asynchronous initialization.
  // As a fallback when no cached auth information is available, try to
  // bootstrap authentication using |lsid|, if it isn't empty.
  // Optionally delete the Sync Data folder (if it's corrupt).
  void Initialize(const GURL& service_url,
                  const syncable::ModelTypeSet& types,
                  URLRequestContextGetter* baseline_context_getter,
                  const std::string& lsid,
                  bool delete_sync_data_folder,
                  bool invalidate_sync_login,
                  bool invalidate_sync_xmpp_login,
                  bool use_chrome_async_socket,
                  NotificationMethod notification_method);

  // Called on |frontend_loop_| to kick off asynchronous authentication.
  void Authenticate(const std::string& username, const std::string& password,
                    const std::string& captcha);

  // This starts the SyncerThread running a Syncer object to communicate with
  // sync servers.  Until this is called, no changes will leave or enter this
  // browser from the cloud / sync servers.
  // Called on |frontend_loop_|.
  virtual void StartSyncingWithServer();

  // Called on |frontend_loop_| to asynchronously set the passphrase.
  void SetPassphrase(const std::string& passphrase);

  // Called on |frontend_loop_| to kick off shutdown.
  // |sync_disabled| indicates if syncing is being disabled or not.
  // See the implementation and Core::DoShutdown for details.
  void Shutdown(bool sync_disabled);

  // Changes the set of data types that are currently being synced.
  // The ready_task will be run when all of the requested data types
  // are up-to-date and ready for activation.  The task will cancelled
  // upon shutdown.  The method takes ownership of the task pointer.
  virtual void ConfigureDataTypes(const syncable::ModelTypeSet& types,
                                  CancelableTask* ready_task);

  // Activates change processing for the given data type.  This must
  // be called synchronously with the data type's model association so
  // no changes are dropped between model association and change
  // processor activation.
  void ActivateDataType(DataTypeController* data_type_controller,
                        ChangeProcessor* change_processor);

  // Deactivates change processing for the given data type.
  void DeactivateDataType(DataTypeController* data_type_controller,
                          ChangeProcessor* change_processor);

  // Requests the backend to pause.  Returns true if the request is
  // sent sucessfully.  When the backend does pause, a SYNC_PAUSED
  // notification is sent to the notification service.
  virtual bool RequestPause();

  // Requests the backend to resume.  Returns true if the request is
  // sent sucessfully.  When the backend does resume, a SYNC_RESUMED
  // notification is sent to the notification service.
  virtual bool RequestResume();

  // Called on |frontend_loop_| to obtain a handle to the UserShare needed
  // for creating transactions.
  UserShareHandle GetUserShareHandle() const;

  // Called from any thread to obtain current status information in detailed or
  // summarized form.
  Status GetDetailedStatus();
  StatusSummary GetStatusSummary();
  const GoogleServiceAuthError& GetAuthError() const;
  const sessions::SyncSessionSnapshot* GetLastSessionSnapshot() const;

  const FilePath& sync_data_folder_path() const {
    return sync_data_folder_path_;
  }

  // Returns the authenticated username of the sync user, or empty if none
  // exists. It will only exist if the authentication service provider (e.g
  // GAIA) has confirmed the username is authentic.
  string16 GetAuthenticatedUsername() const;

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<browser_sync::ModelSafeWorker*>* out);
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out);

#if defined(UNIT_TEST)
  // Called from unit test to bypass authentication and initialize the syncapi
  // to a state suitable for testing but not production.
  void InitializeForTestMode(const std::wstring& test_user,
                             sync_api::HttpPostProviderFactory* factory,
                             sync_api::HttpPostProviderFactory* auth_factory,
                             bool delete_sync_data_folder,
                             NotificationMethod notification_method) {
    if (!core_thread_.Start())
      return;
    registrar_.workers[GROUP_UI] = new UIModelWorker(frontend_loop_);
    registrar_.routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
    registrar_.routing_info[syncable::PREFERENCES] = GROUP_PASSIVE;
    registrar_.routing_info[syncable::AUTOFILL] = GROUP_PASSIVE;
    registrar_.routing_info[syncable::THEMES] = GROUP_PASSIVE;
    registrar_.routing_info[syncable::TYPED_URLS] = GROUP_PASSIVE;
    registrar_.routing_info[syncable::NIGORI] = GROUP_PASSIVE;
    registrar_.routing_info[syncable::PASSWORDS] = GROUP_PASSIVE;

    core_thread_.message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(core_.get(),
        &SyncBackendHost::Core::DoInitializeForTest,
        test_user,
        factory,
        auth_factory,
        delete_sync_data_folder,
        notification_method));
  }
#endif
 protected:
  // InitializationComplete passes through the SyncBackendHost to forward
  // on to |frontend_|, and so that tests can intercept here if they need to
  // set up initial conditions.
  virtual void HandleInitializationCompletedOnFrontendLoop();

  // The real guts of SyncBackendHost, to keep the public client API clean.
  class Core : public base::RefCountedThreadSafe<SyncBackendHost::Core>,
               public sync_api::SyncManager::Observer {
   public:
    explicit Core(SyncBackendHost* backend);

    // SyncManager::Observer implementation.  The Core just acts like an air
    // traffic controller here, forwarding incoming messages to appropriate
    // landing threads.
    virtual void OnChangesApplied(
        syncable::ModelType model_type,
        const sync_api::BaseTransaction* trans,
        const sync_api::SyncManager::ChangeRecord* changes,
        int change_count);
    virtual void OnSyncCycleCompleted(
        const sessions::SyncSessionSnapshot* snapshot);
    virtual void OnInitializationComplete();
    virtual void OnAuthError(const GoogleServiceAuthError& auth_error);
    virtual void OnPassphraseRequired();
    virtual void OnPassphraseAccepted();
    virtual void OnPaused();
    virtual void OnResumed();
    virtual void OnStopSyncingPermanently();

    struct DoInitializeOptions {
      DoInitializeOptions(
          const GURL& service_url,
          bool attempt_last_user_authentication,
          sync_api::HttpPostProviderFactory* http_bridge_factory,
          sync_api::HttpPostProviderFactory* auth_http_bridge_factory,
          const std::string& lsid,
          bool delete_sync_data_folder,
          bool invalidate_sync_login,
          bool invalidate_sync_xmpp_login,
          bool use_chrome_async_socket,
          NotificationMethod notification_method)
          : service_url(service_url),
            attempt_last_user_authentication(attempt_last_user_authentication),
            http_bridge_factory(http_bridge_factory),
            auth_http_bridge_factory(auth_http_bridge_factory),
            lsid(lsid),
            delete_sync_data_folder(delete_sync_data_folder),
            invalidate_sync_login(invalidate_sync_login),
            invalidate_sync_xmpp_login(invalidate_sync_xmpp_login),
            use_chrome_async_socket(use_chrome_async_socket),
            notification_method(notification_method) {}

      GURL service_url;
      bool attempt_last_user_authentication;
      sync_api::HttpPostProviderFactory* http_bridge_factory;
      sync_api::HttpPostProviderFactory* auth_http_bridge_factory;
      std::string lsid;
      bool delete_sync_data_folder;
      bool invalidate_sync_login;
      bool invalidate_sync_xmpp_login;
      bool use_chrome_async_socket;
      NotificationMethod notification_method;
    };

    // Note:
    //
    // The Do* methods are the various entry points from our SyncBackendHost.
    // It calls us on a dedicated thread to actually perform synchronous
    // (and potentially blocking) syncapi operations.
    //
    // Called on the SyncBackendHost core_thread_ to perform initialization
    // of the syncapi on behalf of SyncBackendHost::Initialize.
    void DoInitialize(const DoInitializeOptions& options);

    // Called on our SyncBackendHost's core_thread_ to perform authentication
    // on behalf of SyncBackendHost::Authenticate.
    void DoAuthenticate(const std::string& username,
                        const std::string& password,
                        const std::string& captcha);

    // Called on the SyncBackendHost core_thread_ to tell the syncapi to start
    // syncing (generally after initialization and authentication).
    void DoStartSyncing();

    // Called on the SyncBackendHost core_thread_ to nudge/pause/resume the
    // syncer.
    void DoRequestNudge();
    void DoRequestPause();
    void DoRequestResume();

    // Called on our SyncBackendHost's |core_thread_| to set the passphrase
    // on behalf of SyncBackendHost::SupplyPassphrase.
    void DoSetPassphrase(const std::string& passphrase);

    // The shutdown order is a bit complicated:
    // 1) From |core_thread_|, invoke the syncapi Shutdown call to do a final
    //    SaveChanges, close sqlite handles, and halt the syncer thread (which
    //    could potentially block for 1 minute).
    // 2) Then, from |frontend_loop_|, halt the core_thread_. This causes
    //    syncapi thread-exit handlers to run and make use of cached pointers to
    //    various components owned implicitly by us.
    // 3) Destroy this Core. That will delete syncapi components in a safe order
    //    because the thread that was using them has exited (in step 2).
    void DoShutdown(bool stopping_sync);

    // Set the base request context to use when making HTTP calls.
    // This method will add a reference to the context to persist it
    // on the IO thread. Must be removed from IO thread.

    sync_api::SyncManager* syncapi() { return syncapi_.get(); }

    // Delete the sync data folder to cleanup backend data.  Happens the first
    // time sync is enabled for a user (to prevent accidentally reusing old
    // sync databases), as well as shutdown when you're no longer syncing.
    void DeleteSyncDataFolder();

#if defined(UNIT_TEST)
    // Special form of initialization that does not try and authenticate the
    // last known user (since it will fail in test mode) and does some extra
    // setup to nudge the syncapi into a useable state.
    void DoInitializeForTest(const std::wstring& test_user,
                             sync_api::HttpPostProviderFactory* factory,
                             sync_api::HttpPostProviderFactory* auth_factory,
                             bool delete_sync_data_folder,
                             NotificationMethod notification_method) {
      DoInitialize(DoInitializeOptions(GURL(), false, factory, auth_factory,
                                       std::string(), delete_sync_data_folder,
                                       false, false, false,
                                       notification_method));
        syncapi_->SetupForTestMode(test_user);
    }
#endif

   private:
    friend class base::RefCountedThreadSafe<SyncBackendHost::Core>;

    ~Core() {}

    // Sends a SYNC_PAUSED notification to the notification service on
    // the UI thread.
    void NotifyPaused();

    // Sends a SYNC_RESUMED notification to the notification service
    // on the UI thread.
    void NotifyResumed();

    // Invoked when initialization of syncapi is complete and we can start
    // our timer.
    // This must be called from the thread on which SaveChanges is intended to
    // be run on; the host's |core_thread_|.
    void StartSavingChanges();

    // Invoked periodically to tell the syncapi to persist its state
    // by writing to disk.
    // This is called from the thread we were created on (which is the
    // SyncBackendHost |core_thread_|), using a repeating timer that is kicked
    // off as soon as the SyncManager tells us it completed
    // initialization.
    void SaveChanges();

    // Dispatched to from HandleAuthErrorEventOnCoreLoop to handle updating
    // frontend UI components.
    void HandleAuthErrorEventOnFrontendLoop(
        const GoogleServiceAuthError& new_auth_error);

    // Invoked when a passphrase is required to decrypt a set of Nigori keys.
    void NotifyPassphraseRequired();

    // Invoked when the passphrase provided by the user has been accepted.
    void NotifyPassphraseAccepted();

    // Called from Core::OnSyncCycleCompleted to handle updating frontend
    // thread components.
    void HandleSyncCycleCompletedOnFrontendLoop(
        sessions::SyncSessionSnapshot* snapshot);

    void HandleStopSyncingPermanentlyOnFrontendLoop();

    // Called from Core::OnInitializationComplete to handle updating
    // frontend thread components.
    void HandleInitalizationCompletedOnFrontendLoop();

    // Return true if a model lives on the current thread.
    bool IsCurrentThreadSafeForModel(syncable::ModelType model_type);

    // Our parent SyncBackendHost
    SyncBackendHost* host_;

    // The timer used to periodically call SaveChanges.
    base::RepeatingTimer<Core> save_changes_timer_;

    // The top-level syncapi entry point.
    scoped_ptr<sync_api::SyncManager> syncapi_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Our core, which communicates directly to the syncapi.
  scoped_refptr<Core> core_;

 private:

  UIModelWorker* ui_worker();

  // A thread we dedicate for use by our Core to perform initialization,
  // authentication, handle messages from the syncapi, and periodically tell
  // the syncapi to persist itself.
  base::Thread core_thread_;

  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  MessageLoop* const frontend_loop_;

  Profile* profile_;

  // This is state required to implement ModelSafeWorkerRegistrar.
  struct {
    // We maintain ownership of all workers.  In some cases, we need to ensure
    // shutdown occurs in an expected sequence by Stop()ing certain workers.
    // They are guaranteed to be valid because we only destroy elements of
    // |workers_| after the syncapi has been destroyed.  Unless a worker is no
    // longer needed because all types that get routed to it have been disabled
    // (from syncing). In that case, we'll destroy on demand *after* routing
    // any dependent types to GROUP_PASSIVE, so that the syncapi doesn't call
    // into garbage.  If a key is present, it means at least one ModelType that
    // routes to that model safe group is being synced.
    WorkerMap workers;
    browser_sync::ModelSafeRoutingInfo routing_info;
  } registrar_;

  // The user can incur changes to registrar_ at any time from the UI thread.
  // The syncapi needs to periodically get a consistent snapshot of the state,
  // and it does so from a different thread.  Therefore, we protect creation,
  // destruction, and re-routing events by acquiring this lock.  Note that the
  // SyncBackendHost may read (on the UI thread or core thread) from registrar_
  // without acquiring the lock (which is typically "read ModelSafeWorker
  // pointer value", and then invoke methods), because lifetimes are managed on
  // the UI thread.  Of course, this comment only applies to ModelSafeWorker
  // impls that are themselves thread-safe, such as UIModelWorker.
  Lock registrar_lock_;

  // The frontend which we serve (and are owned by).
  SyncFrontend* frontend_;

  // The change processors that handle the different data types.
  std::map<syncable::ModelType, ChangeProcessor*> processors_;

  // Path of the folder that stores the sync data files.
  FilePath sync_data_folder_path_;

  // List of registered data type controllers.
  DataTypeController::TypeMap data_type_controllers_;

  // A task that should be called once data type configuration is
  // complete.
  scoped_ptr<CancelableTask> configure_ready_task_;

  // The set of types that we are waiting to be initially synced in a
  // configuration cycle.
  syncable::ModelTypeSet configure_initial_sync_types_;

  // UI-thread cache of the last AuthErrorState received from syncapi.
  GoogleServiceAuthError last_auth_error_;

  // UI-thread cache of the last SyncSessionSnapshot received from syncapi.
  scoped_ptr<sessions::SyncSessionSnapshot> last_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHost);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_HOST_H_
