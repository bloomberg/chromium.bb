// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/sync_manager.h"

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_threadsafe.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/nigori_util.h"
#include "chrome/browser/sync/engine/syncapi_internal.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/engine/sync_scheduler.h"
#include "chrome/browser/sync/internal_api/base_node.h"
#include "chrome/browser/sync/internal_api/change_reorder_buffer.h"
#include "chrome/browser/sync/internal_api/configure_reason.h"
#include "chrome/browser/sync/internal_api/debug_info_event_listener.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/syncapi_server_connection_manager.h"
#include "chrome/browser/sync/internal_api/user_share.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_backend.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_event_handler.h"
#include "chrome/browser/sync/js/js_mutation_event_observer.h"
#include "chrome/browser/sync/js/js_reply_handler.h"
#include "chrome/browser/sync/js/js_sync_manager_observer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/browser/sync/protocol/proto_value_conversions.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_change_delegate.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/network_change_notifier.h"

using std::string;

using base::TimeDelta;
using browser_sync::AllStatus;
using browser_sync::Cryptographer;
using browser_sync::JsArgList;
using browser_sync::JsBackend;
using browser_sync::JsEventDetails;
using browser_sync::JsEventHandler;
using browser_sync::JsEventHandler;
using browser_sync::JsReplyHandler;
using browser_sync::JsMutationEventObserver;
using browser_sync::JsSyncManagerObserver;
using browser_sync::ModelSafeWorkerRegistrar;
using browser_sync::kNigoriTag;
using browser_sync::KeyParams;
using browser_sync::ModelSafeRoutingInfo;
using browser_sync::ServerConnectionEvent;
using browser_sync::ServerConnectionEventListener;
using browser_sync::SyncEngineEvent;
using browser_sync::SyncEngineEventListener;
using browser_sync::SyncScheduler;
using browser_sync::Syncer;
using browser_sync::WeakHandle;
using browser_sync::sessions::SyncSessionContext;
using syncable::DirectoryManager;
using syncable::ImmutableWriteTransactionInfo;
using syncable::ModelType;
using syncable::ModelTypeBitSet;
using syncable::SPECIFICS;
using sync_pb::GetUpdatesCallerInfo;

typedef GoogleServiceAuthError AuthError;

namespace {

static const int kSyncSchedulerDelayMsec = 250;

#if defined(OS_CHROMEOS)
static const int kChromeOSNetworkChangeReactionDelayHackMsec = 5000;
#endif  // OS_CHROMEOS

GetUpdatesCallerInfo::GetUpdatesSource GetSourceFromReason(
    sync_api::ConfigureReason reason) {
  switch (reason) {
    case sync_api::CONFIGURE_REASON_RECONFIGURATION:
      return GetUpdatesCallerInfo::RECONFIGURATION;
    case sync_api::CONFIGURE_REASON_MIGRATION:
      return GetUpdatesCallerInfo::MIGRATION;
    case sync_api::CONFIGURE_REASON_NEW_CLIENT:
      return GetUpdatesCallerInfo::NEW_CLIENT;
    case sync_api::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE:
      return GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE;
    default:
      NOTREACHED();
  }

  return GetUpdatesCallerInfo::UNKNOWN;
}

} // namespace

namespace sync_api {

//////////////////////////////////////////////////////////////////////////
// SyncManager's implementation: SyncManager::SyncInternal
class SyncManager::SyncInternal
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public browser_sync::Cryptographer::Observer,
      public sync_notifier::SyncNotifierObserver,
      public JsBackend,
      public SyncEngineEventListener,
      public ServerConnectionEventListener,
      public syncable::DirectoryChangeDelegate {
  static const int kDefaultNudgeDelayMilliseconds;
  static const int kPreferencesNudgeDelayMilliseconds;
 public:
  explicit SyncInternal(const std::string& name)
      : name_(name),
        weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        change_observers_(
            new ObserverListThreadSafe<SyncManager::ChangeObserver>()),
        registrar_(NULL),
        change_delegate_(NULL),
        initialized_(false),
        setup_for_test_mode_(false),
        observing_ip_address_changes_(false),
        created_on_loop_(MessageLoop::current()) {
    // Pre-fill |notification_info_map_|.
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      notification_info_map_.insert(
          std::make_pair(syncable::ModelTypeFromInt(i), NotificationInfo()));
    }

    // Bind message handlers.
    BindJsMessageHandler(
        "getNotificationState",
        &SyncManager::SyncInternal::GetNotificationState);
    BindJsMessageHandler(
        "getNotificationInfo",
        &SyncManager::SyncInternal::GetNotificationInfo);
    BindJsMessageHandler(
        "getRootNodeDetails",
        &SyncManager::SyncInternal::GetRootNodeDetails);
    BindJsMessageHandler(
        "getNodeSummariesById",
        &SyncManager::SyncInternal::GetNodeSummariesById);
    BindJsMessageHandler(
        "getNodeDetailsById",
        &SyncManager::SyncInternal::GetNodeDetailsById);
    BindJsMessageHandler(
        "getChildNodeIds",
        &SyncManager::SyncInternal::GetChildNodeIds);
    BindJsMessageHandler(
        "findNodesContainingString",
        &SyncManager::SyncInternal::FindNodesContainingString);
  }

  virtual ~SyncInternal() {
    CHECK(!initialized_);
  }

  bool Init(const FilePath& database_location,
            const WeakHandle<JsEventHandler>& event_handler,
            const std::string& sync_server_and_path,
            int port,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            ModelSafeWorkerRegistrar* model_safe_worker_registrar,
            ChangeDelegate* change_delegate,
            const std::string& user_agent,
            const SyncCredentials& credentials,
            sync_notifier::SyncNotifier* sync_notifier,
            const std::string& restored_key_for_bootstrapping,
            bool setup_for_test_mode);

  void CheckServerReachable() {
    if (connection_manager()) {
      connection_manager()->CheckServerReachable();
    } else {
      NOTREACHED() << "Should be valid connection manager!";
    }
  }

  // Sign into sync with given credentials.
  // We do not verify the tokens given. After this call, the tokens are set
  // and the sync DB is open. True if successful, false if something
  // went wrong.
  bool SignIn(const SyncCredentials& credentials);

  // Update tokens that we're using in Sync. Email must stay the same.
  void UpdateCredentials(const SyncCredentials& credentials);

  // Called when the user disables or enables a sync type.
  void UpdateEnabledTypes();

  // Conditionally sets the flag in the Nigori node which instructs other
  // clients to start syncing tabs.
  void MaybeSetSyncTabsInNigoriNode(const syncable::ModelTypeSet enabled_types);

  // Tell the sync engine to start the syncing process.
  void StartSyncingNormally();

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  bool IsUsingExplicitPassphrase();

  // Update the Cryptographer from the current nigori node and write back any
  // necessary changes to the nigori node. We also detect missing encryption
  // keys and write them into the nigori node.
  // Note: opens a transaction and can trigger an ON_PASSPHRASE_REQUIRED, so
  // should only be called after syncapi is fully initialized.
  // Returns true if cryptographer is ready, false otherwise.
  bool UpdateCryptographerAndNigori();

  // Updates the nigori node with any new encrypted types and then
  // encrypts the nodes for those new data types as well as other
  // nodes that should be encrypted but aren't.  Triggers
  // OnPassphraseRequired if the cryptographer isn't ready.
  void RefreshEncryption();

  // Try to set the current passphrase to |passphrase|, and record whether
  // it is an explicit passphrase or implicitly using gaia in the Nigori
  // node.
  void SetPassphrase(const std::string& passphrase, bool is_explicit);

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  virtual void HandleTransactionCompleteChangeEvent(
      const ModelTypeBitSet& models_with_changes) OVERRIDE;
  virtual ModelTypeBitSet HandleTransactionEndingChangeEvent(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncApi(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;
  virtual void HandleCalculateChangesChangeEventFromSyncer(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncable::BaseTransaction* trans) OVERRIDE;

  // Listens for notifications from the ServerConnectionManager
  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  // Open the directory named with username_for_share
  bool OpenDirectory();

  // Cryptographer::Observer implementation.
  virtual void OnEncryptedTypesChanged(
      const syncable::ModelTypeSet& encrypted_types,
      bool encrypt_everything) OVERRIDE;

  // SyncNotifierObserver implementation.
  virtual void OnNotificationStateChange(
      bool notifications_enabled) OVERRIDE;

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads) OVERRIDE;

  virtual void StoreState(const std::string& cookie) OVERRIDE;

  void AddChangeObserver(SyncManager::ChangeObserver* observer);
  void RemoveChangeObserver(SyncManager::ChangeObserver* observer);

  void AddObserver(SyncManager::Observer* observer);
  void RemoveObserver(SyncManager::Observer* observer);

  // Accessors for the private members.
  DirectoryManager* dir_manager() { return share_.dir_manager.get(); }
  SyncAPIServerConnectionManager* connection_manager() {
    return connection_manager_.get();
  }
  SyncScheduler* scheduler() { return scheduler_.get(); }
  UserShare* GetUserShare() {
    DCHECK(initialized_);
    return &share_;
  }

  // Return the currently active (validated) username for use with syncable
  // types.
  const std::string& username_for_share() const {
    return share_.name;
  }

  Status GetStatus();

  void RequestNudge(const tracked_objects::Location& nudge_location);

  void RequestNudgeForDataType(
      const tracked_objects::Location& nudge_location,
      const ModelType& type);

  // See SyncManager::Shutdown* for information.
  void StopSyncingForShutdown(const base::Closure& callback);
  void ShutdownOnSyncThread();

  // If this is a deletion for a password, sets the legacy
  // ExtraPasswordChangeRecordData field of |buffer|. Otherwise sets
  // |buffer|'s specifics field to contain the unencrypted data.
  void SetExtraChangeRecordData(int64 id,
                                syncable::ModelType type,
                                ChangeReorderBuffer* buffer,
                                Cryptographer* cryptographer,
                                const syncable::EntryKernel& original,
                                bool existed_before,
                                bool exists_now);

  // Called only by our NetworkChangeNotifier.
  virtual void OnIPAddressChanged() OVERRIDE;

  bool InitialSyncEndedForAllEnabledTypes() {
    syncable::ModelTypeSet types;
    ModelSafeRoutingInfo enabled_types;
    registrar_->GetModelSafeRoutingInfo(&enabled_types);
    for (ModelSafeRoutingInfo::const_iterator i = enabled_types.begin();
        i != enabled_types.end(); ++i) {
      types.insert(i->first);
    }

    return InitialSyncEndedForTypes(types, &share_);
  }

  // SyncEngineEventListener implementation.
  virtual void OnSyncEngineEvent(const SyncEngineEvent& event) OVERRIDE;

  // ServerConnectionEventListener implementation.
  virtual void OnServerConnectionEvent(
      const ServerConnectionEvent& event) OVERRIDE;

  // JsBackend implementation.
  virtual void SetJsEventHandler(
      const WeakHandle<JsEventHandler>& event_handler) OVERRIDE;
  virtual void ProcessJsMessage(
      const std::string& name, const JsArgList& args,
      const WeakHandle<JsReplyHandler>& reply_handler) OVERRIDE;

 private:
  struct NotificationInfo {
    int total_count;
    std::string payload;

    NotificationInfo() : total_count(0) {}

    ~NotificationInfo() {}

    // Returned pointer owned by the caller.
    DictionaryValue* ToValue() const {
      DictionaryValue* value = new DictionaryValue();
      value->SetInteger("totalCount", total_count);
      value->SetString("payload", payload);
      return value;
    }
  };

  typedef std::map<syncable::ModelType, NotificationInfo> NotificationInfoMap;
  typedef JsArgList
      (SyncManager::SyncInternal::*UnboundJsMessageHandler)(const JsArgList&);
  typedef base::Callback<JsArgList(JsArgList)> JsMessageHandler;
  typedef std::map<std::string, JsMessageHandler> JsMessageHandlerMap;

  // Helper to call OnAuthError when no authentication credentials are
  // available.
  void RaiseAuthNeededEvent();

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry stored in |a| and |b|.  Note that a node's index may
  // change without its NEXT_ID changing if the node at NEXT_ID also moved (but
  // the relative order is unchanged).  To handle such cases, we rely on the
  // caller to treat a position update on any sibling as updating the positions
  // of all siblings.
  static bool VisiblePositionsDiffer(
      const syncable::EntryKernelMutation& mutation) {
    const syncable::EntryKernel& a = mutation.original;
    const syncable::EntryKernel& b = mutation.mutated;
    // If the datatype isn't one where the browser model cares about position,
    // don't bother notifying that data model of position-only changes.
    if (!ShouldMaintainPosition(
            syncable::GetModelTypeFromSpecifics(b.ref(SPECIFICS))))
      return false;
    if (a.ref(syncable::NEXT_ID) != b.ref(syncable::NEXT_ID))
      return true;
    if (a.ref(syncable::PARENT_ID) != b.ref(syncable::PARENT_ID))
      return true;
    return false;
  }

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  static bool VisiblePropertiesDiffer(
      const syncable::EntryKernelMutation& mutation,
      Cryptographer* cryptographer) {
    const syncable::EntryKernel& a = mutation.original;
    const syncable::EntryKernel& b = mutation.mutated;
    const sync_pb::EntitySpecifics& a_specifics = a.ref(SPECIFICS);
    const sync_pb::EntitySpecifics& b_specifics = b.ref(SPECIFICS);
    DCHECK_EQ(syncable::GetModelTypeFromSpecifics(a_specifics),
              syncable::GetModelTypeFromSpecifics(b_specifics));
    syncable::ModelType model_type =
        syncable::GetModelTypeFromSpecifics(b_specifics);
    // Suppress updates to items that aren't tracked by any browser model.
    if (model_type < syncable::FIRST_REAL_MODEL_TYPE ||
        !a.ref(syncable::UNIQUE_SERVER_TAG).empty()) {
      return false;
    }
    if (a.ref(syncable::IS_DIR) != b.ref(syncable::IS_DIR))
      return true;
    if (!AreSpecificsEqual(cryptographer,
                           a.ref(syncable::SPECIFICS),
                           b.ref(syncable::SPECIFICS))) {
      return true;
    }
    // We only care if the name has changed if neither specifics is encrypted
    // (encrypted nodes blow away the NON_UNIQUE_NAME).
    if (!a_specifics.has_encrypted() && !b_specifics.has_encrypted() &&
        a.ref(syncable::NON_UNIQUE_NAME) != b.ref(syncable::NON_UNIQUE_NAME))
      return true;
    if (VisiblePositionsDiffer(mutation))
      return true;
    return false;
  }

  bool ChangeBuffersAreEmpty() {
    for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
      if (!change_buffers_[i].IsEmpty())
        return false;
    }
    return true;
  }

  void ReEncryptEverything(WriteTransaction* trans);

  // Called for every notification. This updates the notification statistics
  // to be displayed in about:sync.
  void UpdateNotificationInfo(
      const syncable::ModelTypePayloadMap& type_payloads);

  // Checks for server reachabilty and requests a nudge.
  void OnIPAddressChangedImpl();

  // Helper function used only by the constructor.
  void BindJsMessageHandler(
    const std::string& name, UnboundJsMessageHandler unbound_message_handler);

  // Returned pointer is owned by the caller.
  static DictionaryValue* NotificationInfoToValue(
      const NotificationInfoMap& notification_info);

  // JS message handlers.
  JsArgList GetNotificationState(const JsArgList& args);
  JsArgList GetNotificationInfo(const JsArgList& args);
  JsArgList GetRootNodeDetails(const JsArgList& args);
  JsArgList GetNodeSummariesById(const JsArgList& args);
  JsArgList GetNodeDetailsById(const JsArgList& args);
  JsArgList GetChildNodeIds(const JsArgList& args);
  JsArgList FindNodesContainingString(const JsArgList& args);

  const std::string name_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SyncInternal> weak_ptr_factory_;

  // Thread-safe handle used by
  // HandleCalculateChangesChangeEventFromSyncApi(), which can be
  // called from any thread.  Valid only between between calls to
  // Init() and Shutdown().
  //
  // TODO(akalin): Ideally, we wouldn't need to store this; instead,
  // we'd have another worker class which implements
  // HandleCalculateChangesChangeEventFromSyncApi() and we'd pass it a
  // WeakHandle when we construct it.
  WeakHandle<SyncInternal> weak_handle_this_;

  // We couple the DirectoryManager and username together in a UserShare member
  // so we can return a handle to share_ to clients of the API for use when
  // constructing any transaction type.
  UserShare share_;

  // Even though observers are always added/removed from the sync
  // thread, we still need to use a thread-safe observer list as we
  // can notify from any thread.
  scoped_refptr<ObserverListThreadSafe<SyncManager::ChangeObserver> >
      change_observers_;

  ObserverList<SyncManager::Observer> observers_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  scoped_ptr<SyncAPIServerConnectionManager> connection_manager_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  scoped_ptr<SyncScheduler> scheduler_;

  // The SyncNotifier which notifies us when updates need to be downloaded.
  scoped_ptr<sync_notifier::SyncNotifier> sync_notifier_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // Each element of this array is a store of change records produced by
  // HandleChangeEvent during the CALCULATE_CHANGES step.  The changes are
  // segregated by model type, and are stored here to be processed and
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING
  // step by HandleTransactionEndingChangeEvent. The list is cleared in the
  // TRANSACTION_COMPLETE step by HandleTransactionCompleteChangeEvent.
  ChangeReorderBuffer change_buffers_[syncable::MODEL_TYPE_COUNT];

  // The entity that provides us with information about which types to sync.
  // The instance is shared between the SyncManager and the Syncer.
  ModelSafeWorkerRegistrar* registrar_;

  SyncManager::ChangeDelegate* change_delegate_;

  // Set to true once Init has been called.
  bool initialized_;

  // True if the SyncManager should be running in test mode (no sync
  // scheduler actually communicating with the server).
  bool setup_for_test_mode_;

  // Whether we should respond to an IP address change notification.
  bool observing_ip_address_changes_;

  // Map used to store the notification info to be displayed in
  // about:sync page.
  NotificationInfoMap notification_info_map_;

  // These are for interacting with chrome://sync-internals.
  JsMessageHandlerMap js_message_handlers_;
  WeakHandle<JsEventHandler> js_event_handler_;
  JsSyncManagerObserver js_sync_manager_observer_;
  JsMutationEventObserver js_mutation_event_observer_;

  // This is for keeping track of client events to send to the server.
  DebugInfoEventListener debug_info_event_listener_;

  MessageLoop* const created_on_loop_;
};
const int SyncManager::SyncInternal::kDefaultNudgeDelayMilliseconds = 200;
const int SyncManager::SyncInternal::kPreferencesNudgeDelayMilliseconds = 2000;

SyncManager::ChangeDelegate::~ChangeDelegate() {}

SyncManager::ChangeObserver::~ChangeObserver() {}

SyncManager::Observer::~Observer() {}

SyncManager::SyncManager(const std::string& name)
    : data_(new SyncInternal(name)) {}

SyncManager::Status::Status()
    : summary(INVALID),
      authenticated(false),
      server_up(false),
      server_reachable(false),
      server_broken(false),
      notifications_enabled(false),
      notifications_received(0),
      notifiable_commits(0),
      max_consecutive_errors(0),
      unsynced_count(0),
      conflicting_count(0),
      syncing(false),
      initial_sync_ended(false),
      syncer_stuck(false),
      updates_available(0),
      updates_received(0),
      tombstone_updates_received(0),
      disk_full(false),
      num_local_overwrites_total(0),
      num_server_overwrites_total(0),
      nonempty_get_updates(0),
      empty_get_updates(0),
      useless_sync_cycles(0),
      useful_sync_cycles(0),
      cryptographer_ready(false),
      crypto_has_pending_keys(false) {
}

SyncManager::Status::~Status() {
}

bool SyncManager::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int sync_server_port,
    bool use_ssl,
    HttpPostProviderFactory* post_factory,
    ModelSafeWorkerRegistrar* registrar,
    ChangeDelegate* change_delegate,
    const std::string& user_agent,
    const SyncCredentials& credentials,
    sync_notifier::SyncNotifier* sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    bool setup_for_test_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(post_factory);
  VLOG(1) << "SyncManager starting Init...";
  string server_string(sync_server_and_path);
  return data_->Init(database_location,
                     event_handler,
                     server_string,
                     sync_server_port,
                     use_ssl,
                     post_factory,
                     registrar,
                     change_delegate,
                     user_agent,
                     credentials,
                     sync_notifier,
                     restored_key_for_bootstrapping,
                     setup_for_test_mode);
}

void SyncManager::CheckServerReachable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->CheckServerReachable();
}

void SyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->UpdateCredentials(credentials);
}

void SyncManager::UpdateEnabledTypes() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->UpdateEnabledTypes();
}

void SyncManager::MaybeSetSyncTabsInNigoriNode(
    const syncable::ModelTypeSet enabled_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->MaybeSetSyncTabsInNigoriNode(enabled_types);
}

bool SyncManager::InitialSyncEndedForAllEnabledTypes() {
  return data_->InitialSyncEndedForAllEnabledTypes();
}

void SyncManager::StartSyncingNormally() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->StartSyncingNormally();
}

void SyncManager::SetPassphrase(const std::string& passphrase,
     bool is_explicit) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->SetPassphrase(passphrase, is_explicit);
}

void SyncManager::EnableEncryptEverything() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    // Update the cryptographer to know we're now encrypting everything.
    WriteTransaction trans(FROM_HERE, GetUserShare());
    Cryptographer* cryptographer = trans.GetCryptographer();
    // Only set encrypt everything if we know we can encrypt. This allows the
    // user to cancel encryption if they have forgotten their passphrase.
    if (cryptographer->is_ready())
      cryptographer->set_encrypt_everything();
  }

  // Reads from cryptographer so will automatically encrypt all
  // datatypes and update the nigori node as necessary. Will trigger
  // OnPassphraseRequired if necessary.
  data_->RefreshEncryption();
}

bool SyncManager::EncryptEverythingEnabledForTest() const {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  return trans.GetCryptographer()->encrypt_everything();
}

bool SyncManager::IsUsingExplicitPassphrase() {
  return data_ && data_->IsUsingExplicitPassphrase();
}

void SyncManager::RequestCleanupDisabledTypes() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_->scheduler())
    data_->scheduler()->ScheduleCleanupDisabledTypes();
}

void SyncManager::RequestClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_->scheduler())
    data_->scheduler()->ScheduleClearUserData();
}

void SyncManager::RequestConfig(const syncable::ModelTypeBitSet& types,
    ConfigureReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!data_->scheduler()) {
    LOG(INFO)
        << "SyncManager::RequestConfig: bailing out because scheduler is "
        << "null";
    return;
  }
  StartConfigurationMode(base::Closure());
  data_->scheduler()->ScheduleConfig(types, GetSourceFromReason(reason));
}

void SyncManager::StartConfigurationMode(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!data_->scheduler()) {
    LOG(INFO)
        << "SyncManager::StartConfigurationMode: could not start "
        << "configuration mode because because scheduler is null";
    return;
  }
  data_->scheduler()->Start(
      browser_sync::SyncScheduler::CONFIGURATION_MODE, callback);
}

const std::string& SyncManager::GetAuthenticatedUsername() {
  DCHECK(data_);
  return data_->username_for_share();
}

bool SyncManager::SyncInternal::Init(
    const FilePath& database_location,
    const WeakHandle<JsEventHandler>& event_handler,
    const std::string& sync_server_and_path,
    int port,
    bool use_ssl,
    HttpPostProviderFactory* post_factory,
    ModelSafeWorkerRegistrar* model_safe_worker_registrar,
    ChangeDelegate* change_delegate,
    const std::string& user_agent,
    const SyncCredentials& credentials,
    sync_notifier::SyncNotifier* sync_notifier,
    const std::string& restored_key_for_bootstrapping,
    bool setup_for_test_mode) {
  CHECK(!initialized_);

  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Starting SyncInternal initialization.";

  weak_handle_this_ = MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());

  registrar_ = model_safe_worker_registrar;
  change_delegate_ = change_delegate;
  setup_for_test_mode_ = setup_for_test_mode;

  sync_notifier_.reset(sync_notifier);

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(event_handler);

  AddObserver(&debug_info_event_listener_);

  share_.dir_manager.reset(new DirectoryManager(database_location));

  connection_manager_.reset(new SyncAPIServerConnectionManager(
      sync_server_and_path, port, use_ssl, user_agent, post_factory));

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  observing_ip_address_changes_ = true;

  connection_manager()->AddListener(this);

  // Test mode does not use a syncer context or syncer thread.
  if (!setup_for_test_mode_) {
    // Build a SyncSessionContext and store the worker in it.
    VLOG(1) << "Sync is bringing up SyncSessionContext.";
    std::vector<SyncEngineEventListener*> listeners;
    listeners.push_back(&allstatus_);
    listeners.push_back(this);
    SyncSessionContext* context = new SyncSessionContext(
        connection_manager_.get(),
        dir_manager(),
        model_safe_worker_registrar,
        listeners,
        &debug_info_event_listener_);
    context->set_account_name(credentials.email);
    // The SyncScheduler takes ownership of |context|.
    scheduler_.reset(new SyncScheduler(name_, context, new Syncer()));
  }

  bool signed_in = SignIn(credentials);

  if (signed_in) {
    if (scheduler()) {
      scheduler()->Start(
          browser_sync::SyncScheduler::CONFIGURATION_MODE, base::Closure());
    }

    initialized_ = true;

    // Cryptographer should only be accessed while holding a
    // transaction.  Grabbing the user share for the transaction
    // checks the initialization state, so this must come after
    // |initialized_| is set to true.
    ReadTransaction trans(FROM_HERE, GetUserShare());
    trans.GetCryptographer()->Bootstrap(restored_key_for_bootstrapping);
    trans.GetCryptographer()->AddObserver(this);
  }

  // Notify that initialization is complete. Note: This should be the last to
  // execute if |signed_in| is false. Reason being in that case we would
  // post a task to shutdown sync. But if this function posts any other tasks
  // on the UI thread and if shutdown wins then that tasks would execute on
  // a freed pointer. This is because UI thread is not shut down.
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnInitializationComplete(
                        WeakHandle<JsBackend>(weak_ptr_factory_.GetWeakPtr()),
                        signed_in));

  if (!signed_in && !setup_for_test_mode_)
    return false;

  sync_notifier_->AddObserver(this);

  return signed_in;
}

bool SyncManager::SyncInternal::UpdateCryptographerAndNigori() {
  DCHECK(initialized_);
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    NOTREACHED()
        << "UpdateCryptographerAndNigori: lookup not good so bailing out";
    return false;
  }
  if (!lookup->initial_sync_ended_for_type(syncable::NIGORI))
    return false;  // Should only happen during first time sync.

  WriteTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();

  WriteNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    NOTREACHED();
    return false;
  }
  sync_pb::NigoriSpecifics nigori(node.GetNigoriSpecifics());
  Cryptographer::UpdateResult result = cryptographer->Update(nigori);
  if (result == Cryptographer::NEEDS_PASSPHRASE) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnPassphraseRequired(sync_api::REASON_DECRYPTION));
  }

  // Due to http://crbug.com/102526, we must check if the encryption keys
  // are present in the nigori node. If they're not, we write the current set of
  // keys.
  if (!nigori.has_encrypted() && cryptographer->is_ready()) {
    cryptographer->GetKeys(nigori.mutable_encrypted());
  }

  // Ensure the nigori node reflects the most recent set of sensitive types
  // and properly sets encrypt_everything. This is a no-op if nothing changes.
  cryptographer->UpdateNigoriFromEncryptedTypes(&nigori);
  node.SetNigoriSpecifics(nigori);

  allstatus_.SetCryptographerReady(cryptographer->is_ready());
  allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());
  allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());

  return cryptographer->is_ready();
}

void SyncManager::SyncInternal::StartSyncingNormally() {
  // Start the sync scheduler. This won't actually result in any
  // syncing until at least the DirectoryManager broadcasts the OPENED
  // event, and a valid server connection is detected.
  if (scheduler())  // NULL during certain unittests.
    scheduler()->Start(SyncScheduler::NORMAL_MODE, base::Closure());
}

bool SyncManager::SyncInternal::OpenDirectory() {
  DCHECK(!initialized_) << "Should only happen once";

  bool share_opened = dir_manager()->Open(username_for_share(), this);
  if (!share_opened) {
    LOG(ERROR) << "Could not open share for:" << username_for_share();
    return false;
  }

  // Database has to be initialized for the guid to be available.
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    NOTREACHED();
    return false;
  }

  connection_manager()->set_client_id(lookup->cache_guid());
  lookup->AddTransactionObserver(&js_mutation_event_observer_);
  AddChangeObserver(&js_mutation_event_observer_);
  return true;
}

bool SyncManager::SyncInternal::SignIn(const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(share_.name.empty());
  share_.name = credentials.email;

  VLOG(1) << "Signing in user: " << username_for_share();
  if (!OpenDirectory())
    return false;

  // Retrieve and set the sync notifier state. This should be done
  // only after OpenDirectory is called.
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  std::string unique_id;
  std::string state;
  if (lookup.good()) {
    unique_id = lookup->cache_guid();
    state = lookup->GetNotificationState();
    VLOG(1) << "Read notification unique ID: " << unique_id;
    if (VLOG_IS_ON(1)) {
      std::string encoded_state;
      base::Base64Encode(state, &encoded_state);
      VLOG(1) << "Read notification state: " << encoded_state;
    }
  } else {
    LOG(ERROR) << "Could not read notification unique ID/state";
  }
  sync_notifier_->SetUniqueId(unique_id);
  sync_notifier_->SetState(state);

  UpdateCredentials(credentials);
  UpdateEnabledTypes();
  return true;
}

void SyncManager::SyncInternal::UpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(credentials.email, share_.name);
  DCHECK(!credentials.email.empty());
  DCHECK(!credentials.sync_token.empty());

  observing_ip_address_changes_ = true;
  if (connection_manager()->set_auth_token(credentials.sync_token)) {
    sync_notifier_->UpdateCredentials(
        credentials.email, credentials.sync_token);
    if (!setup_for_test_mode_ && initialized_) {
      // Post a task so we don't block UpdateCredentials.
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&SyncInternal::CheckServerReachable,
                                weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void SyncManager::SyncInternal::UpdateEnabledTypes() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ModelSafeRoutingInfo routes;
  registrar_->GetModelSafeRoutingInfo(&routes);
  syncable::ModelTypeSet enabled_types;
  for (ModelSafeRoutingInfo::const_iterator it = routes.begin();
       it != routes.end(); ++it) {
    enabled_types.insert(it->first);
  }
  sync_notifier_->UpdateEnabledTypes(enabled_types);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSyncTabsForOtherClients)) {
    MaybeSetSyncTabsInNigoriNode(enabled_types);
  }
}

void SyncManager::SyncInternal::MaybeSetSyncTabsInNigoriNode(
    const syncable::ModelTypeSet enabled_types) {
  // The initialized_ check is to ensure that we don't CHECK in GetUserShare
  // when this is called on start-up. It's ok to ignore that case, since
  // presumably this would've run when the user originally enabled sessions.
  if (initialized_ && enabled_types.count(syncable::SESSIONS) > 0) {
    WriteTransaction trans(FROM_HERE, GetUserShare());
    WriteNode node(&trans);
    if (!node.InitByTagLookup(kNigoriTag)) {
      NOTREACHED() << "Unable to set 'sync_tabs' bit because Nigori node not "
                   << "found.";
      return;
    }

    sync_pb::NigoriSpecifics specifics(node.GetNigoriSpecifics());
    specifics.set_sync_tabs(true);
    node.SetNigoriSpecifics(specifics);
  }
}

void SyncManager::SyncInternal::RaiseAuthNeededEvent() {
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
      OnAuthError(AuthError(AuthError::INVALID_GAIA_CREDENTIALS)));
}

void SyncManager::SyncInternal::SetPassphrase(
    const std::string& passphrase, bool is_explicit) {
  // We do not accept empty passphrases.
  if (passphrase.empty()) {
    VLOG(1) << "Rejecting empty passphrase.";
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
        OnPassphraseRequired(sync_api::REASON_SET_PASSPHRASE_FAILED));
    return;
  }

  // All accesses to the cryptographer are protected by a transaction.
  WriteTransaction trans(FROM_HERE, GetUserShare());
  Cryptographer* cryptographer = trans.GetCryptographer();
  KeyParams params = {"localhost", "dummy", passphrase};

  WriteNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return;
  }

  if (cryptographer->has_pending_keys()) {
    bool succeeded = false;

    // See if the explicit flag matches what is set in nigori. If not we dont
    // even try the passphrase. Note: This could mean that we wont try setting
    // the gaia password as passphrase if custom is elected by the user. Which
    // is fine because nigori node has all the old passwords in it.
    if (node.GetNigoriSpecifics().using_explicit_passphrase() == is_explicit) {
      if (cryptographer->DecryptPendingKeys(params)) {
        succeeded = true;
      } else {
        VLOG(1) << "Passphrase failed to decrypt pending keys.";
      }
    } else {
      VLOG(1) << "Not trying the passphrase because the explicit flags dont "
              << "match. Nigori node's explicit flag is "
              << node.GetNigoriSpecifics().using_explicit_passphrase();
    }

    if (!succeeded) {
      FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
          OnPassphraseRequired(sync_api::REASON_SET_PASSPHRASE_FAILED));
      return;
    }

    // Nudge the syncer so that encrypted datatype updates that were waiting for
    // this passphrase get applied as soon as possible.
    RequestNudge(FROM_HERE);
  } else {
    VLOG(1) << "No pending keys, adding provided passphrase.";

    // Prevent an implicit SetPassphrase request from changing an explicitly
    // set passphrase.
    if (!is_explicit && node.GetNigoriSpecifics().using_explicit_passphrase())
      return;

    cryptographer->AddKey(params);

    // TODO(tim): Bug 58231. It would be nice if SetPassphrase didn't require
    // messing with the Nigori node, because we can't call SetPassphrase until
    // download conditions are met vs Cryptographer init.  It seems like it's
    // safe to defer this work.
    sync_pb::NigoriSpecifics specifics(node.GetNigoriSpecifics());
    specifics.clear_encrypted();
    cryptographer->GetKeys(specifics.mutable_encrypted());
    specifics.set_using_explicit_passphrase(is_explicit);
    node.SetNigoriSpecifics(specifics);
  }

  // Does nothing if everything is already encrypted or the cryptographer has
  // pending keys.
  ReEncryptEverything(&trans);

  VLOG(1) << "Passphrase accepted, bootstrapping encryption.";
  std::string bootstrap_token;
  cryptographer->GetBootstrapToken(&bootstrap_token);
  FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                    OnPassphraseAccepted(bootstrap_token));
}

bool SyncManager::SyncInternal::IsUsingExplicitPassphrase() {
  ReadTransaction trans(FROM_HERE, &share_);
  ReadNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    // TODO(albertb): Plumb an UnrecoverableError all the way back to the PSS.
    NOTREACHED();
    return false;
  }

  return node.GetNigoriSpecifics().using_explicit_passphrase();
}

void SyncManager::SyncInternal::RefreshEncryption() {
  DCHECK(initialized_);

  WriteTransaction trans(FROM_HERE, GetUserShare());
  WriteNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    NOTREACHED() << "Unable to set encrypted datatypes because Nigori node not "
                 << "found.";
    return;
  }

  Cryptographer* cryptographer = trans.GetCryptographer();

  if (!cryptographer->is_ready()) {
    VLOG(1) << "Attempting to encrypt datatypes when cryptographer not "
            << "initialized, prompting for passphrase.";
    // TODO(zea): this isn't really decryption, but that's the only way we have
    // to prompt the user for a passsphrase. See http://crbug.com/91379.
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnPassphraseRequired(sync_api::REASON_DECRYPTION));
    return;
  }

  // Update the Nigori node's set of encrypted datatypes.
  // Note, we merge the current encrypted types with those requested. Once a
  // datatypes is marked as needing encryption, it is never unmarked.
  sync_pb::NigoriSpecifics nigori;
  nigori.CopyFrom(node.GetNigoriSpecifics());
  cryptographer->UpdateNigoriFromEncryptedTypes(&nigori);
  node.SetNigoriSpecifics(nigori);
  allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());

  // We reencrypt everything regardless of whether the set of encrypted
  // types changed to ensure that any stray unencrypted entries are overwritten.
  ReEncryptEverything(&trans);
}

// TODO(zea): Add unit tests that ensure no sync changes are made when not
// needed.
void SyncManager::SyncInternal::ReEncryptEverything(WriteTransaction* trans) {
  Cryptographer* cryptographer = trans->GetCryptographer();
  if (!cryptographer || !cryptographer->is_ready())
    return;
  syncable::ModelTypeSet encrypted_types = GetEncryptedTypes(trans);
  ModelSafeRoutingInfo routes;
  registrar_->GetModelSafeRoutingInfo(&routes);
  std::string tag;
  for (syncable::ModelTypeSet::iterator iter = encrypted_types.begin();
       iter != encrypted_types.end(); ++iter) {
    if (*iter == syncable::PASSWORDS ||
        *iter == syncable::NIGORI ||
        routes.count(*iter) == 0)
      continue;
    ReadNode type_root(trans);
    tag = syncable::ModelTypeToRootTag(*iter);
    if (!type_root.InitByTagLookup(tag)) {
      // This can happen when we enable a datatype for the first time on restart
      // (for example when we upgrade) and therefore haven't done the initial
      // download for that type at the time we RefreshEncryption. There's
      // nothing we can do for now, so just move on to the next type.
      continue;
    }

    // Iterate through all children of this datatype.
    std::queue<int64> to_visit;
    int64 child_id = type_root.GetFirstChildId();
    to_visit.push(child_id);
    while (!to_visit.empty()) {
      child_id = to_visit.front();
      to_visit.pop();
      if (child_id == kInvalidId)
        continue;

      WriteNode child(trans);
      if (!child.InitByIdLookup(child_id)) {
        NOTREACHED();
        continue;
      }
      if (child.GetIsFolder()) {
        to_visit.push(child.GetFirstChildId());
      }
      if (child.GetEntry()->Get(syncable::UNIQUE_SERVER_TAG).empty()) {
      // Rewrite the specifics of the node with encrypted data if necessary
      // (only rewrite the non-unique folders).
        child.ResetFromSpecifics();
      }
      to_visit.push(child.GetSuccessorId());
    }
  }

  if (routes.count(syncable::PASSWORDS) > 0) {
    // Passwords are encrypted with their own legacy scheme.
    ReadNode passwords_root(trans);
    std::string passwords_tag =
        syncable::ModelTypeToRootTag(syncable::PASSWORDS);
    // It's possible we'll have the password routing info and not the password
    // root if we attempted to SetPassphrase before passwords was enabled.
    if (passwords_root.InitByTagLookup(passwords_tag)) {
      int64 child_id = passwords_root.GetFirstChildId();
      while (child_id != kInvalidId) {
        WriteNode child(trans);
        if (!child.InitByIdLookup(child_id)) {
          NOTREACHED();
          return;
        }
        child.SetPasswordSpecifics(child.GetPasswordSpecifics());
        child_id = child.GetSuccessorId();
      }
    }
  }

  // NOTE: We notify from within a transaction.
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_, OnEncryptionComplete());
}

SyncManager::~SyncManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete data_;
}

void SyncManager::AddChangeObserver(ChangeObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->AddChangeObserver(observer);
}

void SyncManager::RemoveChangeObserver(ChangeObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->RemoveChangeObserver(observer);
}

void SyncManager::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->AddObserver(observer);
}

void SyncManager::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->RemoveObserver(observer);
}

void SyncManager::StopSyncingForShutdown(const base::Closure& callback) {
  data_->StopSyncingForShutdown(callback);
}

void SyncManager::SyncInternal::StopSyncingForShutdown(
    const base::Closure& callback) {
  VLOG(2) << "StopSyncingForShutdown";
  if (scheduler())  // May be null in tests.
    scheduler()->RequestStop(callback);
  else
    created_on_loop_->PostTask(FROM_HERE, callback);

  if (connection_manager_.get())
    connection_manager_->TerminateAllIO();
}

void SyncManager::ShutdownOnSyncThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->ShutdownOnSyncThread();
}

void SyncManager::SyncInternal::ShutdownOnSyncThread() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent any in-flight method calls from running.  Also
  // invalidates |weak_handle_this_|.
  weak_ptr_factory_.InvalidateWeakPtrs();

  scheduler_.reset();

  SetJsEventHandler(WeakHandle<JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  if (sync_notifier_.get()) {
    sync_notifier_->RemoveObserver(this);
  }
  sync_notifier_.reset();

  if (connection_manager_.get()) {
    connection_manager_->RemoveListener(this);
  }
  connection_manager_.reset();

  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  observing_ip_address_changes_ = false;

  if (initialized_ && dir_manager()) {
    {
      // Cryptographer should only be accessed while holding a
      // transaction.
      ReadTransaction trans(FROM_HERE, GetUserShare());
      trans.GetCryptographer()->RemoveObserver(this);
      trans.GetLookup()->
          RemoveTransactionObserver(&js_mutation_event_observer_);
      RemoveChangeObserver(&js_mutation_event_observer_);
    }
    dir_manager()->FinalSaveChangesForAll();
    dir_manager()->Close(username_for_share());
  }

  // Reset the DirectoryManager and UserSettings so they relinquish sqlite
  // handles to backing files.
  share_.dir_manager.reset();

  setup_for_test_mode_ = false;
  change_delegate_ = NULL;
  registrar_ = NULL;

  initialized_ = false;

  // We reset this here, since only now we know it will not be
  // accessed from other threads (since we shut down everything).
  weak_handle_this_.Reset();
}

void SyncManager::SyncInternal::OnIPAddressChanged() {
  VLOG(1) << "IP address change detected";
  if (!observing_ip_address_changes_) {
    VLOG(1) << "IP address change dropped.";
    return;
  }

#if defined (OS_CHROMEOS)
  // TODO(tim): This is a hack to intentionally lose a race with flimflam at
  // shutdown, so we don't cause shutdown to wait for our http request.
  // http://crosbug.com/8429
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SyncInternal::OnIPAddressChangedImpl,
                 weak_ptr_factory_.GetWeakPtr()),
      kChromeOSNetworkChangeReactionDelayHackMsec);
#else
  OnIPAddressChangedImpl();
#endif  // defined(OS_CHROMEOS)
}

void SyncManager::SyncInternal::OnIPAddressChangedImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CheckServerReachable();
}

void SyncManager::SyncInternal::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  allstatus_.HandleServerConnectionEvent(event);
  if (event.connection_code ==
      browser_sync::HttpResponse::SERVER_CONNECTION_OK) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnAuthError(AuthError::None()));
  }

  if (event.connection_code == browser_sync::HttpResponse::SYNC_AUTH_ERROR) {
    observing_ip_address_changes_ = false;
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
        OnAuthError(AuthError(AuthError::INVALID_GAIA_CREDENTIALS)));
  }

  if (event.connection_code ==
      browser_sync::HttpResponse::SYNC_SERVER_ERROR) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
        OnAuthError(AuthError(AuthError::CONNECTION_FAILED)));
  }
}

void SyncManager::SyncInternal::HandleTransactionCompleteChangeEvent(
    const syncable::ModelTypeBitSet& models_with_changes) {
  // This notification happens immediately after the transaction mutex is
  // released. This allows work to be performed without blocking other threads
  // from acquiring a transaction.
  if (!change_delegate_)
    return;

  // Call commit.
  for (int i = 0; i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    if (models_with_changes.test(type)) {
      change_delegate_->OnChangesComplete(type);
      change_observers_->Notify(
          &SyncManager::ChangeObserver::OnChangesComplete, type);
    }
  }
}

ModelTypeBitSet SyncManager::SyncInternal::HandleTransactionEndingChangeEvent(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  // This notification happens immediately before a syncable WriteTransaction
  // falls out of scope. It happens while the channel mutex is still held,
  // and while the transaction mutex is held, so it cannot be re-entrant.
  if (!change_delegate_ || ChangeBuffersAreEmpty())
    return ModelTypeBitSet();

  // This will continue the WriteTransaction using a read only wrapper.
  // This is the last chance for read to occur in the WriteTransaction
  // that's closing. This special ReadTransaction will not close the
  // underlying transaction.
  ReadTransaction read_trans(GetUserShare(), trans);

  syncable::ModelTypeBitSet models_with_changes;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    if (change_buffers_[type].IsEmpty())
      continue;

    ImmutableChangeRecordList ordered_changes;
    // TODO(akalin): Propagate up the error further (see
    // http://crbug.com/100907).
    CHECK(change_buffers_[type].GetAllChangesInTreeOrder(&read_trans,
                                                         &ordered_changes));
    if (!ordered_changes.Get().empty()) {
      change_delegate_->
          OnChangesApplied(type, &read_trans, ordered_changes);
      change_observers_->Notify(
          &SyncManager::ChangeObserver::OnChangesApplied,
          type, write_transaction_info.Get().id, ordered_changes);
      models_with_changes.set(i, true);
    }
    change_buffers_[i].Clear();
  }
  return models_with_changes;
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncApi(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  if (!scheduler()) {
    return;
  }

  // We have been notified about a user action changing a sync model.
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  // The mutated model type, or UNSPECIFIED if nothing was mutated.
  syncable::ModelType mutated_model_type = syncable::UNSPECIFIED;

  // Find the first real mutation.  We assume that only a single model
  // type is mutated per transaction.
  const syncable::ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (syncable::EntryKernelMutationMap::const_iterator it =
           mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    if (!it->second.mutated.ref(syncable::IS_UNSYNCED)) {
      continue;
    }

    syncable::ModelType model_type =
        syncable::GetModelTypeFromSpecifics(
            it->second.mutated.ref(SPECIFICS));
    if (model_type < syncable::FIRST_REAL_MODEL_TYPE) {
      NOTREACHED() << "Permanent or underspecified item changed via syncapi.";
      continue;
    }

    // Found real mutation.
    if (mutated_model_type == syncable::UNSPECIFIED) {
      mutated_model_type = model_type;
      break;
    }
  }

  // Nudge if necessary.
  if (mutated_model_type != syncable::UNSPECIFIED) {
    if (weak_handle_this_.IsInitialized()) {
      weak_handle_this_.Call(FROM_HERE,
                             &SyncInternal::RequestNudgeForDataType,
                             FROM_HERE,
                             mutated_model_type);
    } else {
      NOTREACHED();
    }
  }
}

void SyncManager::SyncInternal::SetExtraChangeRecordData(int64 id,
    syncable::ModelType type, ChangeReorderBuffer* buffer,
    Cryptographer* cryptographer, const syncable::EntryKernel& original,
    bool existed_before, bool exists_now) {
  // If this is a deletion and the datatype was encrypted, we need to decrypt it
  // and attach it to the buffer.
  if (!exists_now && existed_before) {
    sync_pb::EntitySpecifics original_specifics(original.ref(SPECIFICS));
    if (type == syncable::PASSWORDS) {
      // Passwords must use their own legacy ExtraPasswordChangeRecordData.
      scoped_ptr<sync_pb::PasswordSpecificsData> data(
          DecryptPasswordSpecifics(original_specifics, cryptographer));
      if (!data.get()) {
        NOTREACHED();
        return;
      }
      buffer->SetExtraDataForId(id, new ExtraPasswordChangeRecordData(*data));
    } else if (original_specifics.has_encrypted()) {
      // All other datatypes can just create a new unencrypted specifics and
      // attach it.
      const sync_pb::EncryptedData& encrypted = original_specifics.encrypted();
      if (!cryptographer->Decrypt(encrypted, &original_specifics)) {
        NOTREACHED();
        return;
      }
    }
    buffer->SetSpecificsForId(id, original_specifics);
  }
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncer(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncable::BaseTransaction* trans) {
  // We only expect one notification per sync step, so change_buffers_ should
  // contain no pending entries.
  LOG_IF(WARNING, !ChangeBuffersAreEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  Cryptographer* crypto = dir_manager()->GetCryptographer(trans);
  const syncable::ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (syncable::EntryKernelMutationMap::const_iterator it =
           mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    bool existed_before = !it->second.original.ref(syncable::IS_DEL);
    bool exists_now = !it->second.mutated.ref(syncable::IS_DEL);

    // Omit items that aren't associated with a model.
    syncable::ModelType type =
        syncable::GetModelTypeFromSpecifics(
            it->second.mutated.ref(SPECIFICS));
    if (type < syncable::FIRST_REAL_MODEL_TYPE)
      continue;

    int64 handle = it->first;
    if (exists_now && !existed_before)
      change_buffers_[type].PushAddedItem(handle);
    else if (!exists_now && existed_before)
      change_buffers_[type].PushDeletedItem(handle);
    else if (exists_now && existed_before &&
             VisiblePropertiesDiffer(it->second, crypto)) {
      change_buffers_[type].PushUpdatedItem(
          handle, VisiblePositionsDiffer(it->second));
    }

    SetExtraChangeRecordData(handle, type, &change_buffers_[type], crypto,
                             it->second.original, existed_before, exists_now);
  }
}

SyncManager::Status SyncManager::SyncInternal::GetStatus() {
  return allstatus_.status();
}

void SyncManager::SyncInternal::RequestNudge(
    const tracked_objects::Location& location) {
  if (scheduler())
     scheduler()->ScheduleNudge(
        TimeDelta::FromMilliseconds(0), browser_sync::NUDGE_SOURCE_LOCAL,
        ModelTypeBitSet(), location);
}

void SyncManager::SyncInternal::RequestNudgeForDataType(
    const tracked_objects::Location& nudge_location,
    const ModelType& type) {
  if (!scheduler()) {
    NOTREACHED();
    return;
  }
  base::TimeDelta nudge_delay;
  switch (type) {
    case syncable::PREFERENCES:
      nudge_delay =
          TimeDelta::FromMilliseconds(kPreferencesNudgeDelayMilliseconds);
      break;
    case syncable::SESSIONS:
      nudge_delay = scheduler()->sessions_commit_delay();
      break;
    default:
      nudge_delay =
          TimeDelta::FromMilliseconds(kDefaultNudgeDelayMilliseconds);
      break;
  }
  syncable::ModelTypeBitSet types;
  types.set(type);
  scheduler()->ScheduleNudge(nudge_delay,
                             browser_sync::NUDGE_SOURCE_LOCAL,
                             types,
                             nudge_location);
}

void SyncManager::SyncInternal::OnSyncEngineEvent(
    const SyncEngineEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    ModelSafeRoutingInfo enabled_types;
    registrar_->GetModelSafeRoutingInfo(&enabled_types);
    {
      // Check to see if we need to notify the frontend that we have newly
      // encrypted types or that we require a passphrase.
      sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
      Cryptographer* cryptographer = trans.GetCryptographer();
      // If we've completed a sync cycle and the cryptographer isn't ready
      // yet, prompt the user for a passphrase.
      if (cryptographer->has_pending_keys()) {
        VLOG(1) << "OnPassPhraseRequired Sent";
        FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                          OnPassphraseRequired(sync_api::REASON_DECRYPTION));
      } else if (!cryptographer->is_ready() &&
                 event.snapshot->initial_sync_ended.test(syncable::NIGORI)) {
        VLOG(1) << "OnPassphraseRequired sent because cryptographer is not "
                << "ready";
        FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                          OnPassphraseRequired(sync_api::REASON_ENCRYPTION));
      }

      allstatus_.SetCryptographerReady(cryptographer->is_ready());
      allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());
      allstatus_.SetEncryptedTypes(cryptographer->GetEncryptedTypes());
    }

    if (!initialized_) {
      LOG(INFO) << "OnSyncCycleCompleted not sent because sync api is not "
                << "initialized";
      return;
    }

    if (!event.snapshot->has_more_to_sync) {
      VLOG(1) << "Sending OnSyncCycleCompleted";
      FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                        OnSyncCycleCompleted(event.snapshot));
    }

    // This is here for tests, which are still using p2p notifications.
    //
    // TODO(chron): Consider changing this back to track has_more_to_sync
    // only notify peers if a successful commit has occurred.
    bool is_notifiable_commit =
        (event.snapshot->syncer_status.num_successful_commits > 0);
    if (is_notifiable_commit) {
      allstatus_.IncrementNotifiableCommits();
      if (sync_notifier_.get()) {
        const syncable::ModelTypeSet& changed_types =
            syncable::ModelTypePayloadMapToSet(event.snapshot->source.types);
        sync_notifier_->SendNotification(changed_types);
      } else {
        VLOG(1) << "Not sending notification: sync_notifier_ is NULL";
      }
    }
  }

  if (event.what_happened == SyncEngineEvent::STOP_SYNCING_PERMANENTLY) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnStopSyncingPermanently());
    return;
  }

  if (event.what_happened == SyncEngineEvent::CLEAR_SERVER_DATA_SUCCEEDED) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnClearServerDataSucceeded());
    return;
  }

  if (event.what_happened == SyncEngineEvent::CLEAR_SERVER_DATA_FAILED) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnClearServerDataFailed());
    return;
  }

  if (event.what_happened == SyncEngineEvent::UPDATED_TOKEN) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnUpdatedToken(event.updated_token));
    return;
  }

  if (event.what_happened == SyncEngineEvent::ACTIONABLE_ERROR) {
    FOR_EACH_OBSERVER(SyncManager::Observer, observers_,
                      OnActionableError(
                          event.snapshot->errors.sync_protocol_error));
    return;
  }

}

void SyncManager::SyncInternal::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  js_event_handler_ = event_handler;
  js_sync_manager_observer_.SetJsEventHandler(js_event_handler_);
  js_mutation_event_observer_.SetJsEventHandler(js_event_handler_);
}

void SyncManager::SyncInternal::ProcessJsMessage(
    const std::string& name, const JsArgList& args,
    const WeakHandle<JsReplyHandler>& reply_handler) {
  if (!initialized_) {
    NOTREACHED();
    return;
  }

  if (!reply_handler.IsInitialized()) {
    VLOG(1) << "Uninitialized reply handler; dropping unknown message "
            << name << " with args " << args.ToString();
    return;
  }

  JsMessageHandler js_message_handler = js_message_handlers_[name];
  if (js_message_handler.is_null()) {
    VLOG(1) << "Dropping unknown message " << name
              << " with args " << args.ToString();
    return;
  }

  reply_handler.Call(FROM_HERE,
                     &JsReplyHandler::HandleJsReply,
                     name, js_message_handler.Run(args));
}

void SyncManager::SyncInternal::BindJsMessageHandler(
    const std::string& name,
    UnboundJsMessageHandler unbound_message_handler) {
  js_message_handlers_[name] =
      base::Bind(unbound_message_handler, base::Unretained(this));
}

DictionaryValue* SyncManager::SyncInternal::NotificationInfoToValue(
    const NotificationInfoMap& notification_info) {
  DictionaryValue* value = new DictionaryValue();

  for (NotificationInfoMap::const_iterator it = notification_info.begin();
      it != notification_info.end(); ++it) {
    const std::string& model_type_str =
        syncable::ModelTypeToString(it->first);
    value->Set(model_type_str, it->second.ToValue());
  }

  return value;
}

JsArgList SyncManager::SyncInternal::GetNotificationState(
    const JsArgList& args) {
  bool notifications_enabled = allstatus_.status().notifications_enabled;
  ListValue return_args;
  return_args.Append(Value::CreateBooleanValue(notifications_enabled));
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetNotificationInfo(
    const JsArgList& args) {
  ListValue return_args;
  return_args.Append(NotificationInfoToValue(notification_info_map_));
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::GetRootNodeDetails(
    const JsArgList& args) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode root(&trans);
  root.InitByRootLookup();
  ListValue return_args;
  return_args.Append(root.GetDetailsAsValue());
  return JsArgList(&return_args);
}

namespace {

int64 GetId(const ListValue& ids, int i) {
  std::string id_str;
  if (!ids.GetString(i, &id_str)) {
    return kInvalidId;
  }
  int64 id = kInvalidId;
  if (!base::StringToInt64(id_str, &id)) {
    return kInvalidId;
  }
  return id;
}

JsArgList GetNodeInfoById(const JsArgList& args,
                          UserShare* user_share,
                          DictionaryValue* (BaseNode::*info_getter)() const) {
  CHECK(info_getter);
  ListValue return_args;
  ListValue* node_summaries = new ListValue();
  return_args.Append(node_summaries);
  ListValue* id_list = NULL;
  ReadTransaction trans(FROM_HERE, user_share);
  if (args.Get().GetList(0, &id_list)) {
    CHECK(id_list);
    for (size_t i = 0; i < id_list->GetSize(); ++i) {
      int64 id = GetId(*id_list, i);
      if (id == kInvalidId) {
        continue;
      }
      ReadNode node(&trans);
      if (!node.InitByIdLookup(id)) {
        continue;
      }
      node_summaries->Append((node.*info_getter)());
    }
  }
  return JsArgList(&return_args);
}

}  // namespace

JsArgList SyncManager::SyncInternal::GetNodeSummariesById(
    const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetSummaryAsValue);
}

JsArgList SyncManager::SyncInternal::GetNodeDetailsById(
    const JsArgList& args) {
  return GetNodeInfoById(args, GetUserShare(), &BaseNode::GetDetailsAsValue);
}

JsArgList SyncManager::SyncInternal::GetChildNodeIds(
    const JsArgList& args) {
  ListValue return_args;
  ListValue* child_ids = new ListValue();
  return_args.Append(child_ids);
  int64 id = GetId(args.Get(), 0);
  if (id != kInvalidId) {
    ReadTransaction trans(FROM_HERE, GetUserShare());
    syncable::Directory::ChildHandles child_handles;
    trans.GetLookup()->GetChildHandlesByHandle(trans.GetWrappedTrans(),
                                               id, &child_handles);
    for (syncable::Directory::ChildHandles::const_iterator it =
             child_handles.begin(); it != child_handles.end(); ++it) {
      child_ids->Append(Value::CreateStringValue(
          base::Int64ToString(*it)));
    }
  }
  return JsArgList(&return_args);
}

JsArgList SyncManager::SyncInternal::FindNodesContainingString(
    const JsArgList& args) {
  std::string query;
  ListValue return_args;
  if (!args.Get().GetString(0, &query)) {
    return_args.Append(new ListValue());
    return JsArgList(&return_args);
  }

  // Convert the query string to lower case to perform case insensitive
  // searches.
  std::string lowercase_query = query;
  StringToLowerASCII(&lowercase_query);

  ListValue* result = new ListValue();
  return_args.Append(result);

  ReadTransaction trans(FROM_HERE, GetUserShare());
  std::vector<const syncable::EntryKernel*> entry_kernels;
  trans.GetLookup()->GetAllEntryKernels(trans.GetWrappedTrans(),
                                        &entry_kernels);

  for (std::vector<const syncable::EntryKernel*>::const_iterator it =
           entry_kernels.begin(); it != entry_kernels.end(); ++it) {
    if ((*it)->ContainsString(lowercase_query)) {
      result->Append(new StringValue(base::Int64ToString(
          (*it)->ref(syncable::META_HANDLE))));
    }
  }

  return JsArgList(&return_args);
}

void SyncManager::SyncInternal::OnEncryptedTypesChanged(
    const syncable::ModelTypeSet& encrypted_types,
    bool encrypt_everything) {
  // NOTE: We're in a transaction.
  FOR_EACH_OBSERVER(
      SyncManager::Observer, observers_,
      OnEncryptedTypesChanged(encrypted_types, encrypt_everything));
}

void SyncManager::SyncInternal::OnNotificationStateChange(
    bool notifications_enabled) {
  VLOG(1) << "P2P: Notifications enabled = "
          << (notifications_enabled ? "true" : "false");
  allstatus_.SetNotificationsEnabled(notifications_enabled);
  if (scheduler()) {
    scheduler()->set_notifications_enabled(notifications_enabled);
  }
  if (js_event_handler_.IsInitialized()) {
    DictionaryValue details;
    details.Set("enabled", Value::CreateBooleanValue(notifications_enabled));
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onNotificationStateChange",
                           JsEventDetails(&details));
  }
}

void SyncManager::SyncInternal::UpdateNotificationInfo(
    const syncable::ModelTypePayloadMap& type_payloads) {
  for (syncable::ModelTypePayloadMap::const_iterator it = type_payloads.begin();
       it != type_payloads.end(); ++it) {
    NotificationInfo* info = &notification_info_map_[it->first];
    info->total_count++;
    info->payload = it->second;
  }
}

void SyncManager::SyncInternal::OnIncomingNotification(
    const syncable::ModelTypePayloadMap& type_payloads) {
  if (!type_payloads.empty()) {
    if (scheduler()) {
      scheduler()->ScheduleNudgeWithPayloads(
          TimeDelta::FromMilliseconds(kSyncSchedulerDelayMsec),
          browser_sync::NUDGE_SOURCE_NOTIFICATION,
          type_payloads, FROM_HERE);
    }
    allstatus_.IncrementNotificationsReceived();
    UpdateNotificationInfo(type_payloads);
  } else {
    LOG(WARNING) << "Sync received notification without any type information.";
  }

  if (js_event_handler_.IsInitialized()) {
    DictionaryValue details;
    ListValue* changed_types = new ListValue();
    details.Set("changedTypes", changed_types);
    for (syncable::ModelTypePayloadMap::const_iterator
             it = type_payloads.begin();
         it != type_payloads.end(); ++it) {
      const std::string& model_type_str =
          syncable::ModelTypeToString(it->first);
      changed_types->Append(Value::CreateStringValue(model_type_str));
    }
    js_event_handler_.Call(FROM_HERE,
                           &JsEventHandler::HandleJsEvent,
                           "onIncomingNotification",
                           JsEventDetails(&details));
  }
}

void SyncManager::SyncInternal::StoreState(
    const std::string& state) {
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    LOG(ERROR) << "Could not write notification state";
    // TODO(akalin): Propagate result callback all the way to this
    // function and call it with "false" to signal failure.
    return;
  }
  if (VLOG_IS_ON(1)) {
    std::string encoded_state;
    base::Base64Encode(state, &encoded_state);
    VLOG(1) << "Writing notification state: " << encoded_state;
  }
  lookup->SetNotificationState(state);
  lookup->SaveChanges();
}

void SyncManager::SyncInternal::AddChangeObserver(
    SyncManager::ChangeObserver* observer) {
  change_observers_->AddObserver(observer);
}

void SyncManager::SyncInternal::RemoveChangeObserver(
    SyncManager::ChangeObserver* observer) {
  change_observers_->RemoveObserver(observer);
}

void SyncManager::SyncInternal::AddObserver(
    SyncManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncManager::SyncInternal::RemoveObserver(
    SyncManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

SyncManager::Status::Summary SyncManager::GetStatusSummary() const {
  return data_->GetStatus().summary;
}

SyncManager::Status SyncManager::GetDetailedStatus() const {
  return data_->GetStatus();
}

void SyncManager::SaveChanges() {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->SaveChanges();
}

void SyncManager::SyncInternal::SaveChanges() {
  syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
  if (!lookup.good()) {
    DCHECK(false) << "ScopedDirLookup creation failed; Unable to SaveChanges";
    return;
  }
  lookup->SaveChanges();
}

UserShare* SyncManager::GetUserShare() const {
  return data_->GetUserShare();
}

void SyncManager::RefreshEncryption() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_->UpdateCryptographerAndNigori())
    data_->RefreshEncryption();
}

syncable::ModelTypeSet SyncManager::GetEncryptedDataTypesForTest() const {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  return GetEncryptedTypes(&trans);
}

bool SyncManager::ReceivedExperimentalTypes(syncable::ModelTypeSet* to_add)
    const {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  ReadNode node(&trans);
  if (!node.InitByTagLookup(kNigoriTag)) {
    VLOG(1) << "Couldn't find Nigori node.";
    return false;
  }
  if (node.GetNigoriSpecifics().sync_tabs()) {
    to_add->insert(syncable::SESSIONS);
    return true;
  }
  return false;
}

bool SyncManager::HasUnsyncedItems() const {
  sync_api::ReadTransaction trans(FROM_HERE, GetUserShare());
  return (trans.GetWrappedTrans()->directory()->unsynced_entity_count() != 0);
}

void SyncManager::TriggerOnNotificationStateChangeForTest(
    bool notifications_enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_->OnNotificationStateChange(notifications_enabled);
}

void SyncManager::TriggerOnIncomingNotificationForTest(
    const syncable::ModelTypeBitSet& model_types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncable::ModelTypePayloadMap model_types_with_payloads =
      syncable::ModelTypePayloadMapFromBitSet(model_types,
          std::string());

  data_->OnIncomingNotification(model_types_with_payloads);
}

// Helper function that converts a PassphraseRequiredReason value to a string.
std::string PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason) {
  switch (reason) {
    case REASON_PASSPHRASE_NOT_REQUIRED:
      return "REASON_PASSPHRASE_NOT_REQUIRED";
    case REASON_ENCRYPTION:
      return "REASON_ENCRYPTION";
    case REASON_DECRYPTION:
      return "REASON_DECRYPTION";
    case REASON_SET_PASSPHRASE_FAILED:
      return "REASON_SET_PASSPHRASE_FAILED";
    default:
      NOTREACHED();
      return "INVALID_REASON";
  }
}

// Helper function to determine if initial sync had ended for types.
bool InitialSyncEndedForTypes(syncable::ModelTypeSet types,
                              sync_api::UserShare* share) {
  syncable::ScopedDirLookup lookup(share->dir_manager.get(),
                                   share->name);
  if (!lookup.good()) {
    DCHECK(false) << "ScopedDirLookup failed when checking initial sync";
    return false;
  }

  for (syncable::ModelTypeSet::const_iterator i = types.begin();
       i != types.end(); ++i) {
    if (!lookup->initial_sync_ended_for_type(*i))
      return false;
  }
  return true;
}

syncable::ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
    const syncable::ModelTypeSet types,
    sync_api::UserShare* share) {
  syncable::ScopedDirLookup lookup(share->dir_manager.get(),
                                   share->name);
  if (!lookup.good()) {
    NOTREACHED() << "ScopedDirLookup failed for "
                  << "GetTypesWithEmptyProgressMarkerToken";
    return syncable::ModelTypeSet();
  }

  syncable::ModelTypeSet result;
  for (syncable::ModelTypeSet::const_iterator i = types.begin();
       i != types.end(); ++i) {
    sync_pb::DataTypeProgressMarker marker;
    lookup->GetDownloadProgress(*i, &marker);

    if (marker.token().empty())
      result.insert(*i);

  }
  return result;
}

}  // namespace sync_api
