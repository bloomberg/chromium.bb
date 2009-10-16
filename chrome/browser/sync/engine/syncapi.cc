// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncapi.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <iphlpapi.h>
#endif

#include <iomanip>
#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/platform_thread.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/change_reorder_buffer.h"
#include "chrome/browser/sync/engine/client_command_channel.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/gaia_authenticator.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/net/syncapi_server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator_impl.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/closure.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "chrome/browser/sync/util/path_helpers.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
#pragma comment(lib, "iphlpapi.lib")
#endif

using browser_sync::AllStatus;
using browser_sync::AllStatusEvent;
using browser_sync::AuthWatcher;
using browser_sync::AuthWatcherEvent;
using browser_sync::ClientCommandChannel;
using browser_sync::Syncer;
using browser_sync::SyncerEvent;
using browser_sync::SyncerStatus;
using browser_sync::SyncerThread;
using browser_sync::SyncerThreadFactory;
using browser_sync::UserSettings;
using browser_sync::TalkMediator;
using browser_sync::TalkMediatorImpl;
using std::list;
using std::hex;
using std::string;
using std::vector;
using syncable::Directory;
using syncable::DirectoryManager;

static const int kServerReachablePollingIntervalMsec = 60000 * 60;
static const int kThreadExitTimeoutMsec = 60000;
static const int kSSLPort = 443;

struct AddressWatchTaskParams {
  browser_sync::ServerConnectionManager* conn_mgr;
#if defined(OS_WIN)
  HANDLE exit_flag;
#endif
};

// This thread calls CheckServerReachable() whenever a change occurs in the
// table that maps IP addresses to interfaces, for example when the user
// unplugs his network cable.
class AddressWatchTask : public Task {
 public:
  explicit AddressWatchTask(AddressWatchTaskParams* params)
      : params_(params) {}
  virtual ~AddressWatchTask() {}

  virtual void Run() {
    LOG(INFO) << "starting the address watch thread";
#if defined(OS_WIN)
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    HANDLE file;
    DWORD rc = WAIT_OBJECT_0;
    while (true) {
      // Only call NotifyAddrChange() after the IP address has changed or if
      // this is the first time through the loop.
      if (WAIT_OBJECT_0 == rc) {
        ResetEvent(overlapped.hEvent);
        DWORD notify_result = NotifyAddrChange(&file, &overlapped);
        if (ERROR_IO_PENDING != notify_result) {
          LOG(ERROR) << "NotifyAddrChange() returned unexpected result "
              << hex << notify_result;
          break;
        }
      }
      HANDLE events[] = { overlapped.hEvent, params_->exit_flag };
      rc = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE,
                                  kServerReachablePollingIntervalMsec);

      // If the exit flag was signaled, the thread will exit.
      if (WAIT_OBJECT_0 + 1 == rc)
        break;

      params_->conn_mgr->CheckServerReachable();
    }
    CloseHandle(overlapped.hEvent);
#else
  // TODO(zork): Add this functionality to Linux.
#endif
    LOG(INFO) << "The address watch thread has stopped";
  }

 private:
  AddressWatchTaskParams* const params_;
  DISALLOW_COPY_AND_ASSIGN(AddressWatchTask);
};

namespace sync_api {
class ModelSafeWorkerBridge;

static const PSTR_CHAR kBookmarkSyncUserSettingsDatabase[] =
    PSTR("BookmarkSyncSettings.sqlite3");
static const PSTR_CHAR kDefaultNameForNewNodes[] = PSTR(" ");

// The list of names which are reserved for use by the server.
static const char16* kForbiddenServerNames[] =
    { STRING16(""), STRING16("."), STRING16("..") };

//////////////////////////////////////////////////////////////////////////
// Static helper functions.

// Helper function to look up the int64 metahandle of an object given the ID
// string.
static int64 IdToMetahandle(syncable::BaseTransaction* trans,
                            const syncable::Id& id) {
  syncable::Entry entry(trans, syncable::GET_BY_ID, id);
  if (!entry.good())
    return kInvalidId;
  return entry.Get(syncable::META_HANDLE);
}

// Checks whether |name| is a server-illegal name followed by zero or more space
// characters.  The three server-illegal names are the empty string, dot, and
// dot-dot.  Very long names (>255 bytes in UTF-8 Normalization Form C) are
// also illegal, but are not considered here.
static bool IsNameServerIllegalAfterTrimming(const string16& name) {
  size_t untrimmed_count = name.find_last_not_of(' ') + 1;
  for (size_t i = 0; i < arraysize(kForbiddenServerNames); ++i) {
    if (name.compare(0, untrimmed_count, kForbiddenServerNames[i]) == 0)
      return true;
  }
  return false;
}

static bool EndsWithSpace(const string16& string) {
  return !string.empty() && *string.rbegin() == ' ';
}

static inline void String16ToPathString(const sync_char16 *in,
                                        PathString *out) {
  string16 in_str(in);
#if defined(OS_WIN)
  out->assign(in_str);
#else
  UTF16ToUTF8(in_str.c_str(), in_str.length(), out);
#endif
}

static inline void PathStringToString16(const PathString& in, string16* out) {
#if defined(OS_WIN)
  out->assign(in);
#else
  UTF8ToUTF16(in.c_str(), in.length(), out);
#endif
}

// When taking a name from the syncapi, append a space if it matches the
// pattern of a server-illegal name followed by zero or more spaces.
static void SyncAPINameToServerName(const sync_char16 *sync_api_name,
                                    PathString* out) {
  String16ToPathString(sync_api_name, out);
  string16 sync_api_name_str(sync_api_name);
  if (IsNameServerIllegalAfterTrimming(sync_api_name_str))
    out->append(PSTR(" "));
}

// In the reverse direction, if a server name matches the pattern of a
// server-illegal name followed by one or more spaces, remove the trailing
// space.
static void ServerNameToSyncAPIName(const PathString& server_name,
                                    string16*out) {
  string16 server_name_str;
  PathStringToString16(server_name, &server_name_str);
  if (IsNameServerIllegalAfterTrimming(server_name_str) &&
      EndsWithSpace(server_name_str))
    out->assign(server_name_str, 0, server_name_str.size() - 1);
  else
    out->assign(server_name_str);
}

// A UserShare encapsulates the syncable pieces that represent an authenticated
// user and their data (share).
// This encompasses all pieces required to build transaction objects on the
// syncable share.
struct UserShare {
  // The DirectoryManager itself, which is the parent of Transactions and can
  // be shared across multiple threads (unlike Directory).
  scoped_ptr<DirectoryManager> dir_manager;

  // The username of the sync user. This is empty until we have performed at
  // least one successful GAIA authentication with this username, which means
  // on first-run it is empty until an AUTH_SUCCEEDED event and on future runs
  // it is set as soon as the client instructs us to authenticate for the last
  // known valid user (AuthenticateForLastKnownUser()).
  // Stored as a PathString to avoid string conversions each time a transaction
  // is created.
  PathString authenticated_name;
};

////////////////////////////////////
// BaseNode member definitions.

// BaseNode::BaseNodeInternal provides storage for member Get() functions that
// need to return pointers (e.g. strings).
struct BaseNode::BaseNodeInternal {
  string16 url;
  string16 title;
  Directory::ChildHandles child_handles;
  syncable::Blob favicon;
};

BaseNode::BaseNode() : data_(new BaseNode::BaseNodeInternal) {}

BaseNode::~BaseNode() {
  delete data_;
}

int64 BaseNode::GetParentId() const {
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(),
                        GetEntry()->Get(syncable::PARENT_ID));
}

int64 BaseNode::GetId() const {
  return GetEntry()->Get(syncable::META_HANDLE);
}

bool BaseNode::GetIsFolder() const {
  return GetEntry()->Get(syncable::IS_DIR);
}

const sync_char16* BaseNode::GetTitle() const {
  // Store the string in data_ so that the returned pointer is valid.
  ServerNameToSyncAPIName(GetEntry()->GetName().non_unique_value(),
                         &data_->title);
  return data_->title.c_str();
}

const sync_char16* BaseNode::GetURL() const {
  // Store the string in data_ so that the returned pointer is valid.
  PathStringToString16(GetEntry()->Get(syncable::BOOKMARK_URL), &data_->url);
  return data_->url.c_str();
}

const int64* BaseNode::GetChildIds(size_t* child_count) const {
  DCHECK(child_count);
  Directory* dir = GetTransaction()->GetLookup();
  dir->GetChildHandles(GetTransaction()->GetWrappedTrans(),
                       GetEntry()->Get(syncable::ID), &data_->child_handles);

  *child_count = data_->child_handles.size();
  return (data_->child_handles.empty()) ? NULL : &data_->child_handles[0];
}

int64 BaseNode::GetPredecessorId() const {
  syncable::Id id_string = GetEntry()->Get(syncable::PREV_ID);
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64 BaseNode::GetSuccessorId() const {
  syncable::Id id_string = GetEntry()->Get(syncable::NEXT_ID);
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

int64 BaseNode::GetFirstChildId() const {
  syncable::Directory* dir = GetTransaction()->GetLookup();
  syncable::BaseTransaction* trans = GetTransaction()->GetWrappedTrans();
  syncable::Id id_string =
      dir->GetFirstChildId(trans, GetEntry()->Get(syncable::ID));
  if (id_string.IsRoot())
    return kInvalidId;
  return IdToMetahandle(GetTransaction()->GetWrappedTrans(), id_string);
}

const unsigned char* BaseNode::GetFaviconBytes(size_t* size_in_bytes) {
  data_->favicon = GetEntry()->Get(syncable::BOOKMARK_FAVICON);
  *size_in_bytes = data_->favicon.size();
  if (*size_in_bytes)
    return &(data_->favicon[0]);
  else
    return NULL;
}

int64 BaseNode::GetExternalId() const {
  return GetEntry()->Get(syncable::LOCAL_EXTERNAL_ID);
}

////////////////////////////////////
// WriteNode member definitions
void WriteNode::SetIsFolder(bool folder) {
  if (entry_->Get(syncable::IS_DIR) == folder)
    return;  // Skip redundant changes.

  entry_->Put(syncable::IS_DIR, folder);
  MarkForSyncing();
}

void WriteNode::SetTitle(const sync_char16* title) {
  PathString server_legal_name;
  SyncAPINameToServerName(title, &server_legal_name);

  syncable::Name old_name = entry_->GetName();

  if (server_legal_name == old_name.non_unique_value())
    return;  // Skip redundant changes.

  // Otherwise, derive a new unique name so we have a valid value
  // to use as the DBName.
  syncable::SyncName sync_name(server_legal_name);
  syncable::DBName db_name(sync_name.value());
  db_name.MakeOSLegal();
  db_name.MakeNoncollidingForEntry(transaction_->GetWrappedTrans(),
                                   entry_->Get(syncable::PARENT_ID), entry_);

  syncable::Name new_name = syncable::Name::FromDBNameAndSyncName(db_name,
                                                                  sync_name);
  entry_->PutName(new_name);
  MarkForSyncing();
}

void WriteNode::SetURL(const sync_char16* url) {
  PathString url_string;
  String16ToPathString(url, &url_string);
  if (url_string == entry_->Get(syncable::BOOKMARK_URL))
    return;  // Skip redundant changes.

  entry_->Put(syncable::BOOKMARK_URL, url_string);
  MarkForSyncing();
}

void WriteNode::SetExternalId(int64 id) {
  if (GetExternalId() != id)
    entry_->Put(syncable::LOCAL_EXTERNAL_ID, id);
}

WriteNode::WriteNode(WriteTransaction* transaction)
    : entry_(NULL), transaction_(transaction) {
  DCHECK(transaction);
}

WriteNode::~WriteNode() {
  delete entry_;
}

// Find an existing node matching the ID |id|, and bind this WriteNode to it.
// Return true on success.
bool WriteNode::InitByIdLookup(int64 id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::GET_BY_HANDLE, id);
  return (entry_->good() && !entry_->Get(syncable::IS_DEL));
}

// Create a new node with default properties, and bind this WriteNode to it.
// Return true on success.
bool WriteNode::InitByCreation(const BaseNode& parent,
                               const BaseNode* predecessor) {
  DCHECK(!entry_) << "Init called twice";
  // |predecessor| must be a child of |parent| or NULL.
  if (predecessor && predecessor->GetParentId() != parent.GetId()) {
    DCHECK(false);
    return false;
  }

  syncable::Id parent_id = parent.GetEntry()->Get(syncable::ID);

  // Start out with a dummy name, but make it unique.  We expect
  // the caller to set a meaningful name after creation.
  syncable::DBName dummy(kDefaultNameForNewNodes);
  dummy.MakeOSLegal();
  dummy.MakeNoncollidingForEntry(transaction_->GetWrappedTrans(), parent_id,
                                 NULL);

  entry_ = new syncable::MutableEntry(transaction_->GetWrappedWriteTrans(),
                                      syncable::CREATE, parent_id, dummy);

  if (!entry_->good())
    return false;

  // Entries are untitled folders by default.
  entry_->Put(syncable::IS_DIR, true);
  // TODO(ncarter): Naming this bit IS_BOOKMARK_OBJECT is a bit unfortunate,
  // since the rest of SyncAPI is essentially bookmark-agnostic.
  entry_->Put(syncable::IS_BOOKMARK_OBJECT, true);

  // Now set the predecessor, which sets IS_UNSYNCED as necessary.
  PutPredecessor(predecessor);

  return true;
}

bool WriteNode::SetPosition(const BaseNode& new_parent,
                            const BaseNode* predecessor) {
  // |predecessor| must be a child of |new_parent| or NULL.
  if (predecessor && predecessor->GetParentId() != new_parent.GetId()) {
    DCHECK(false);
    return false;
  }

  syncable::Id new_parent_id = new_parent.GetEntry()->Get(syncable::ID);

  // Filter out redundant changes if both the parent and the predecessor match.
  if (new_parent_id == entry_->Get(syncable::PARENT_ID)) {
    const syncable::Id& old = entry_->Get(syncable::PREV_ID);
    if ((!predecessor && old.IsRoot()) ||
        (predecessor && (old == predecessor->GetEntry()->Get(syncable::ID)))) {
      return true;
    }
  }

  // Discard the old database name, derive a new database name from the sync
  // name, and make it legal and unique.
  syncable::Name name = syncable::Name::FromSyncName(GetEntry()->GetName());
  name.db_value().MakeOSLegal();
  name.db_value().MakeNoncollidingForEntry(GetTransaction()->GetWrappedTrans(),
                                           new_parent_id, entry_);

  // Atomically change the parent and name. This will fail if it would
  // introduce a cycle in the hierarchy.
  if (!entry_->PutParentIdAndName(new_parent_id, name))
    return false;

  // Now set the predecessor, which sets IS_UNSYNCED as necessary.
  PutPredecessor(predecessor);

  return true;
}

const syncable::Entry* WriteNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* WriteNode::GetTransaction() const {
  return transaction_;
}

void WriteNode::Remove() {
  entry_->Put(syncable::IS_DEL, true);
  MarkForSyncing();
}

void WriteNode::PutPredecessor(const BaseNode* predecessor) {
  syncable::Id predecessor_id = predecessor ?
      predecessor->GetEntry()->Get(syncable::ID) : syncable::Id();
  entry_->PutPredecessor(predecessor_id);
  // Mark this entry as unsynced, to wake up the syncer.
  MarkForSyncing();
}

void WriteNode::SetFaviconBytes(const unsigned char* bytes,
                                size_t size_in_bytes) {
  syncable::Blob new_favicon(bytes, bytes + size_in_bytes);
  if (new_favicon == entry_->Get(syncable::BOOKMARK_FAVICON))
    return;  // Skip redundant changes.

  entry_->Put(syncable::BOOKMARK_FAVICON, new_favicon);
  MarkForSyncing();
}

void WriteNode::MarkForSyncing() {
  syncable::MarkForSyncing(entry_);
}

//////////////////////////////////////////////////////////////////////////
// ReadNode member definitions
ReadNode::ReadNode(const BaseTransaction* transaction)
    : entry_(NULL), transaction_(transaction) {
  DCHECK(transaction);
}

ReadNode::~ReadNode() {
  delete entry_;
}

void ReadNode::InitByRootLookup() {
  DCHECK(!entry_) << "Init called twice";
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_ID, trans->root_id());
  if (!entry_->good())
    DCHECK(false) << "Could not lookup root node for reading.";
}

bool ReadNode::InitByIdLookup(int64 id) {
  DCHECK(!entry_) << "Init called twice";
  DCHECK_NE(id, kInvalidId);
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_HANDLE, id);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  LOG_IF(WARNING, !entry_->Get(syncable::IS_BOOKMARK_OBJECT))
      << "SyncAPI InitByIdLookup referencing non-bookmark object.";
  return true;
}

const syncable::Entry* ReadNode::GetEntry() const {
  return entry_;
}

const BaseTransaction* ReadNode::GetTransaction() const {
  return transaction_;
}

bool ReadNode::InitByTagLookup(const sync_char16* tag) {
  DCHECK(!entry_) << "Init called twice";
  PathString tag_string;
  String16ToPathString(tag, &tag_string);
  if (tag_string.empty())
    return false;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_TAG, tag_string);
  if (!entry_->good())
    return false;
  if (entry_->Get(syncable::IS_DEL))
    return false;
  LOG_IF(WARNING, !entry_->Get(syncable::IS_BOOKMARK_OBJECT))
      << "SyncAPI InitByTagLookup referencing non-bookmark object.";
  return true;
}


//////////////////////////////////////////////////////////////////////////
// ReadTransaction member definitions
ReadTransaction::ReadTransaction(UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::ReadTransaction(GetLookup(), __FILE__, __LINE__);
}

ReadTransaction::~ReadTransaction() {
  delete transaction_;
}

syncable::BaseTransaction* ReadTransaction::GetWrappedTrans() const {
  return transaction_;
}

//////////////////////////////////////////////////////////////////////////
// WriteTransaction member definitions
WriteTransaction::WriteTransaction(UserShare* share)
    : BaseTransaction(share),
      transaction_(NULL) {
  transaction_ = new syncable::WriteTransaction(GetLookup(), syncable::SYNCAPI,
                                                __FILE__, __LINE__);
}

WriteTransaction::~WriteTransaction() {
  delete transaction_;
}

syncable::BaseTransaction* WriteTransaction::GetWrappedTrans() const {
  return transaction_;
}

// An implementation of Visitor that we use to "visit" the
// ModelSafeWorkerInterface provided by a client of this API. The object we
// visit is responsible for calling DoWork, which will invoke Run() on it's
// cached work closure.
class ModelSafeWorkerVisitor : public ModelSafeWorkerInterface::Visitor {
 public:
  explicit ModelSafeWorkerVisitor(Closure* work) : work_(work) { }
  virtual ~ModelSafeWorkerVisitor() { }

  // ModelSafeWorkerInterface::Visitor implementation.
  virtual void DoWork() {
    work_->Run();
  }

 private:
  // The work to be done. We run this on DoWork and it cleans itself up
  // after it is run.
  Closure* work_;

  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorkerVisitor);
};

// This class is declared in the cc file to allow inheritance from sync types.
// The ModelSafeWorkerBridge is a liason between a syncapi-client defined
// ModelSafeWorkerInterface and the actual ModelSafeWorker used by the Syncer
// for the current SyncManager.
class ModelSafeWorkerBridge : public browser_sync::ModelSafeWorker {
 public:
  // Takes ownership of |worker|.
  explicit ModelSafeWorkerBridge(ModelSafeWorkerInterface* worker)
      : worker_(worker) {
  }
  virtual ~ModelSafeWorkerBridge() { }

  // Overriding ModelSafeWorker.
  virtual void DoWorkAndWaitUntilDone(Closure* work) {
    // When the syncer has work to be done, we forward it to our worker who
    // will invoke DoWork on |visitor| when appropriate (from model safe
    // thread).
    ModelSafeWorkerVisitor visitor(work);
    worker_->CallDoWorkFromModelSafeThreadAndWait(&visitor);
  }

 private:
  // The worker that we can forward work requests to, to ensure the work
  // is performed on an appropriate model safe thread.
  scoped_ptr<ModelSafeWorkerInterface> worker_;

  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorkerBridge);
};

// A GaiaAuthenticator that uses HttpPostProviders instead of CURL.
class BridgedGaiaAuthenticator : public browser_sync::GaiaAuthenticator {
 public:
  BridgedGaiaAuthenticator(const string& user_agent, const string& service_id,
                           const string& gaia_url,
                           HttpPostProviderFactory* factory)
      : GaiaAuthenticator(user_agent, service_id, gaia_url),
        gaia_source_(user_agent), post_factory_(factory) {
  }

  virtual ~BridgedGaiaAuthenticator() {
  }

  virtual bool Post(const GURL& url, const string& post_body,
                    unsigned long* response_code, string* response_body) {
    string connection_url = "https://";
    connection_url += url.host();
    connection_url += url.path();
    HttpPostProviderInterface* http = post_factory_->Create();
    http->SetUserAgent(gaia_source_.c_str());
    // SSL is on 443 for Gaia Posts always.
    http->SetURL(connection_url.c_str(), kSSLPort);
    http->SetPostPayload("application/x-www-form-urlencoded",
                         post_body.length(), post_body.c_str());

    int os_error_code = 0;
    int int_response_code = 0;
    if (!http->MakeSynchronousPost(&os_error_code, &int_response_code)) {
      LOG(INFO) << "Http POST failed, error returns: " << os_error_code;
      return false;
    }
    *response_code = static_cast<int>(int_response_code);
    response_body->assign(http->GetResponseContent(),
                          http->GetResponseContentLength());
    post_factory_->Destroy(http);
    return true;
  }
 private:
  const std::string gaia_source_;
  scoped_ptr<HttpPostProviderFactory> post_factory_;
  DISALLOW_COPY_AND_ASSIGN(BridgedGaiaAuthenticator);
};

//////////////////////////////////////////////////////////////////////////
// SyncManager's implementation: SyncManager::SyncInternal
class SyncManager::SyncInternal {
 public:
  explicit SyncInternal(SyncManager* sync_manager)
      : observer_(NULL),
        command_channel_(0),
        auth_problem_(AUTH_PROBLEM_NONE),
        sync_manager_(sync_manager),
        address_watch_thread_("SyncEngine_AddressWatcher"),
        notification_pending_(false),
        initialized_(false) {
  }

  ~SyncInternal() { }

  bool Init(const PathString& database_location,
            const std::string& sync_server_and_path,
            int port,
            const char* gaia_service_id,
            const char* gaia_source,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            HttpPostProviderFactory* auth_post_factory,
            ModelSafeWorkerInterface* model_safe_worker,
            bool attempt_last_user_authentication,
            const char* user_agent);

  // Tell sync engine to submit credentials to GAIA for verification and start
  // the syncing process on success. Successful GAIA authentication will kick
  // off the following chain of events:
  // 1. Cause sync engine to open the syncer database.
  // 2. Trigger the AuthWatcher to create a Syncer for the directory and call
  //    SyncerThread::SyncDirectory; the SyncerThread will block until (4).
  // 3. Tell the ServerConnectionManager to pass the newly received GAIA auth
  //    token to a sync server to obtain a sync token.
  // 4. On receipt of this token, the ServerConnectionManager broadcasts
  //    a server-reachable event, which will unblock the SyncerThread,
  //    and the rest is the future.
  //
  // If authentication fails, an event will be broadcast all the way up to
  // the SyncManager::Observer. It may, in turn, decide to try again with new
  // credentials. Calling this method again is the appropriate course of action
  // to "retry".
  void Authenticate(const std::string& username, const std::string& password);

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  void HandleChangeEvent(const syncable::DirectoryChangeEvent& event);
  void HandleTransactionCompleteChangeEvent(
      const syncable::DirectoryChangeEvent& event);
  void HandleCalculateChangesChangeEventFromSyncApi(
      const syncable::DirectoryChangeEvent& event);
  void HandleCalculateChangesChangeEventFromSyncer(
      const syncable::DirectoryChangeEvent& event);

  // This listener is called by the syncer channel for all syncer events.
  void HandleSyncerEvent(const SyncerEvent& event);

  // We have a direct hookup to the authwatcher to be notified for auth failures
  // on startup, to serve our UI needs.
  void HandleAuthWatcherEvent(const AuthWatcherEvent& event);

  // Accessors for the private members.
  DirectoryManager* dir_manager() { return share_.dir_manager.get(); }
  SyncAPIServerConnectionManager* connection_manager() {
    return connection_manager_.get();
  }
  SyncerThread* syncer_thread() { return syncer_thread_.get(); }
  TalkMediator* talk_mediator() { return talk_mediator_.get(); }
  AuthWatcher* auth_watcher() { return auth_watcher_.get(); }
  AllStatus* allstatus() { return &allstatus_; }
  void set_observer(Observer* observer) { observer_ = observer; }
  UserShare* GetUserShare() { return &share_; }

  // Return the currently active (validated) username as a PathString for
  // use with syncable types.
  const PathString& username_for_share() const {
    return share_.authenticated_name;
  }

  // Returns the authenticated username from our AuthWatcher in UTF8.
  // See SyncManager::GetAuthenticatedUsername for details.
  const char* GetAuthenticatedUsername();

  // Note about SyncManager::Status implementation: Status is a trimmed
  // down AllStatus::Status, augmented with authentication failure information
  // gathered from the internal AuthWatcher. The sync UI itself hooks up to
  // various sources like the AuthWatcher individually, but with syncapi we try
  // to keep everything status-related in one place. This means we have to
  // privately manage state about authentication failures, and whenever the
  // status or status summary is requested we aggregate this state with
  // AllStatus::Status information.
  Status ComputeAggregatedStatus();
  Status::Summary ComputeAggregatedStatusSummary();

  // See SyncManager::SetupForTestMode for information.
  void SetupForTestMode(const sync_char16* test_username);

  // See SyncManager::Shutdown for information.
  void Shutdown();

  // Whether we're initialized to the point of being able to accept changes
  // (and hence allow transaction creation). See initialized_ for details.
  bool initialized() const {
    AutoLock lock(initialized_mutex_);
    return initialized_;
  }
 private:
  // Try to authenticate using persisted credentials from a previous successful
  // authentication. If no such credentials exist, calls OnAuthError on the
  // client to collect credentials. Otherwise, there exist local credentials
  // that were once used for a successful auth, so we'll try to re-use these.
  // Failure of that attempt will be communicated as normal using OnAuthError.
  // Since this entry point will bypass normal GAIA authentication and try to
  // authenticate directly with the sync service using a cached token,
  // authentication failure will generally occur due to expired credentials, or
  // possibly because of a password change.
  void AuthenticateForLastKnownUser();

  // Helper to call OnAuthError when no authentication credentials are
  // available.
  void RaiseAuthNeededEvent();

  // Helper to set initialized_ to true and raise an event to clients to notify
  // that initialization is complete and it is safe to send us changes. If
  // already initialized, this is a no-op.
  void MarkAndNotifyInitializationComplete();

  // Determine if the parents or predecessors differ between the old and new
  // versions of an entry stored in |a| and |b|.  Note that a node's index may
  // change without its NEXT_ID changing if the node at NEXT_ID also moved (but
  // the relative order is unchanged).  To handle such cases, we rely on the
  // caller to treat a position update on any sibling as updating the positions
  // of all siblings.
  static bool BookmarkPositionsDiffer(const syncable::EntryKernel& a,
                                      const syncable::Entry& b) {
    if (a.ref(syncable::NEXT_ID) != b.Get(syncable::NEXT_ID))
      return true;
    if (a.ref(syncable::PARENT_ID) != b.Get(syncable::PARENT_ID))
      return true;
    return false;
  }

  // Determine if any of the fields made visible to clients of the Sync API
  // differ between the versions of an entry stored in |a| and |b|. A return
  // value of false means that it should be OK to ignore this change.
  static bool BookmarkPropertiesDiffer(const syncable::EntryKernel& a,
                                       const syncable::Entry& b) {
    if (a.ref(syncable::NAME) != b.Get(syncable::NAME))
      return true;
    if (a.ref(syncable::UNSANITIZED_NAME) != b.Get(syncable::UNSANITIZED_NAME))
      return true;
    if (a.ref(syncable::IS_DIR) != b.Get(syncable::IS_DIR))
      return true;
    if (a.ref(syncable::BOOKMARK_URL) != b.Get(syncable::BOOKMARK_URL))
      return true;
    if (a.ref(syncable::BOOKMARK_FAVICON) != b.Get(syncable::BOOKMARK_FAVICON))
      return true;
    if (BookmarkPositionsDiffer(a, b))
      return true;
    return false;
  }

  // We couple the DirectoryManager and username together in a UserShare member
  // so we can return a handle to share_ to clients of the API for use when
  // constructing any transaction type.
  UserShare share_;

  // A cached string for callers of GetAuthenticatedUsername. We just store the
  // last result of auth_watcher_->email() here and change it on future calls,
  // because callers of GetAuthenticatedUsername are supposed to copy the value
  // if they need it for longer than the scope of the call.
  std::string cached_auth_watcher_email_;

  // A wrapper around a sqlite store used for caching authentication data,
  // last user information, current sync-related URLs, and more.
  scoped_ptr<UserSettings> user_settings_;

  // Observer registered via SetObserver/RemoveObserver.
  // WARNING: This can be NULL!
  Observer* observer_;

  // A sink for client commands from the syncer needed to create a SyncerThread.
  ClientCommandChannel command_channel_;

  // The ServerConnectionManager used to abstract communication between the
  // client (the Syncer) and the sync server.
  scoped_ptr<SyncAPIServerConnectionManager> connection_manager_;

  // The thread that runs the Syncer. Needs to be explicitly Start()ed.
  scoped_refptr<SyncerThread> syncer_thread_;

  // Notification (xmpp) handler.
  scoped_ptr<TalkMediator> talk_mediator_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  AllStatus allstatus_;

  // AuthWatcher kicks off the authentication process and follows it through
  // phase 1 (GAIA) to phase 2 (sync engine). As part of this work it determines
  // the initial connectivity and causes the server connection event to be
  // broadcast, which signals the syncer thread to start syncing.
  // It has a heavy duty constructor requiring boilerplate so we heap allocate.
  scoped_refptr<AuthWatcher> auth_watcher_;

  // A store of change records produced by HandleChangeEvent during the
  // CALCULATE_CHANGES step, and to be processed, and forwarded to the
  // observer, by HandleChangeEvent during the TRANSACTION_COMPLETE step.
  ChangeReorderBuffer change_buffer_;

  // The event listener hookup that is registered for HandleChangeEvent.
  scoped_ptr<EventListenerHookup> dir_change_hookup_;

  // The event listener hookup registered for HandleSyncerEvent.
  scoped_ptr<EventListenerHookup> syncer_event_;

  // The event listener hookup registered for HandleAuthWatcherEvent.
  scoped_ptr<EventListenerHookup> authwatcher_hookup_;

  // Our cache of a recent authentication problem. If no authentication problem
  // occurred, or if the last problem encountered has been cleared (by a
  // subsequent AuthWatcherEvent), this is set to AUTH_PROBLEM_NONE.
  AuthProblem auth_problem_;

  // The sync dir_manager to which we belong.
  SyncManager* const sync_manager_;

  // Parameters for our thread listening to network status changes.
  base::Thread address_watch_thread_;
  AddressWatchTaskParams address_watch_params_;

  // True if the next SyncCycle should notify peers of an update.
  bool notification_pending_;

  // Set to true once Init has been called, and we know of an authenticated
  // valid) username either from a fresh authentication attempt (as in
  // first-use case) or from a previous attempt stored in our UserSettings
  // (as in the steady-state), and the syncable::Directory has been opened,
  // meaning we are ready to accept changes.  Protected by initialized_mutex_
  // as it can get read/set by both the SyncerThread and the AuthWatcherThread.
  bool initialized_;
  mutable Lock initialized_mutex_;
};

SyncManager::SyncManager() {
  data_ = new SyncInternal(this);
}

bool SyncManager::Init(const sync_char16* database_location,
                       const char* sync_server_and_path,
                       int sync_server_port,
                       const char* gaia_service_id,
                       const char* gaia_source,
                       bool use_ssl,
                       HttpPostProviderFactory* post_factory,
                       HttpPostProviderFactory* auth_post_factory,
                       ModelSafeWorkerInterface* model_safe_worker,
                       bool attempt_last_user_authentication,
                       const char* user_agent) {
  DCHECK(database_location);
  DCHECK(post_factory);

  PathString db_path;
  String16ToPathString(database_location, &db_path);
  string server_string(sync_server_and_path);
  return data_->Init(db_path,
                     server_string,
                     sync_server_port,
                     gaia_service_id,
                     gaia_source,
                     use_ssl,
                     post_factory,
                     auth_post_factory,
                     model_safe_worker,
                     attempt_last_user_authentication,
                     user_agent);
}

void SyncManager::Authenticate(const char* username, const char* password) {
  data_->Authenticate(std::string(username), std::string(password));
}

const char* SyncManager::GetAuthenticatedUsername() {
  if (!data_)
    return NULL;
  return data_->GetAuthenticatedUsername();
}

const char* SyncManager::SyncInternal::GetAuthenticatedUsername() {
  cached_auth_watcher_email_ = browser_sync::ToUTF8(
      username_for_share()).get_string();
  return cached_auth_watcher_email_.c_str();
}

bool SyncManager::SyncInternal::Init(
    const PathString& database_location,
    const std::string& sync_server_and_path,
    int port,
    const char* gaia_service_id,
    const char* gaia_source,
    bool use_ssl, HttpPostProviderFactory* post_factory,
    HttpPostProviderFactory* auth_post_factory,
    ModelSafeWorkerInterface* model_safe_worker,
    bool attempt_last_user_authentication,
    const char* user_agent) {

  // Set up UserSettings, creating the db if necessary. We need this to
  // instantiate a URLFactory to give to the Syncer.
  PathString settings_db_file = AppendSlash(database_location) +
                                kBookmarkSyncUserSettingsDatabase;
  user_settings_.reset(new UserSettings());
  if (!user_settings_->Init(settings_db_file))
    return false;

  share_.dir_manager.reset(new DirectoryManager(database_location));

  string client_id = user_settings_->GetClientId();
  connection_manager_.reset(new SyncAPIServerConnectionManager(
      sync_server_and_path, port, use_ssl, user_agent, client_id));

  // TODO(timsteele): This is temporary windows crap needed to listen for
  // network status changes. We should either pump this up to the embedder to
  // do (and call us in CheckServerReachable, for ex), or at least make this
  // platform independent in here.
#if defined(OS_WIN)
  HANDLE exit_flag = CreateEvent(NULL, TRUE /*manual reset*/, FALSE, NULL);
  address_watch_params_.exit_flag = exit_flag;
#endif
  address_watch_params_.conn_mgr = connection_manager();

  bool address_watch_started = address_watch_thread_.Start();
  DCHECK(address_watch_started);
  address_watch_thread_.message_loop()->PostTask(FROM_HERE,
      new AddressWatchTask(&address_watch_params_));

  // Hand over the bridged POST factory to be owned by the connection
  // dir_manager.
  connection_manager()->SetHttpPostProviderFactory(post_factory);

  // Watch various objects for aggregated status.
  allstatus()->WatchConnectionManager(connection_manager());

  std::string gaia_url = browser_sync::kGaiaUrl;
  const char* service_id = gaia_service_id ?
      gaia_service_id : SYNC_SERVICE_NAME;

  talk_mediator_.reset(new TalkMediatorImpl());
  allstatus()->WatchTalkMediator(talk_mediator());

  BridgedGaiaAuthenticator* gaia_auth = new BridgedGaiaAuthenticator(
      gaia_source, service_id, gaia_url, auth_post_factory);

  auth_watcher_ = new AuthWatcher(dir_manager(),
                                  connection_manager(),
                                  &allstatus_,
                                  gaia_source,
                                  service_id,
                                  gaia_url,
                                  user_settings_.get(),
                                  gaia_auth,
                                  talk_mediator());

  talk_mediator()->WatchAuthWatcher(auth_watcher());
  allstatus()->WatchAuthWatcher(auth_watcher());
  authwatcher_hookup_.reset(NewEventListenerHookup(auth_watcher_->channel(),
      this, &SyncInternal::HandleAuthWatcherEvent));

  // Tell the SyncerThread to use the ModelSafeWorker for bookmark model work.
  // We set up both sides of the "bridge" here, with the ModelSafeWorkerBridge
  // on the Syncer side, and |model_safe_worker| on the API client side.
  ModelSafeWorkerBridge* worker = new ModelSafeWorkerBridge(model_safe_worker);

  syncer_thread_ = SyncerThreadFactory::Create(&command_channel_,
                                               dir_manager(),
                                               connection_manager(),
                                               &allstatus_,
                                               worker);
  syncer_thread()->WatchTalkMediator(talk_mediator());
  allstatus()->WatchSyncerThread(syncer_thread());

  syncer_thread()->Start();  // Start the syncer thread. This won't actually
                             // result in any syncing until at least the
                             // DirectoryManager broadcasts the OPENED event,
                             // and a valid server connection is detected.

  if (attempt_last_user_authentication)
    AuthenticateForLastKnownUser();
  return true;
}

void SyncManager::SyncInternal::MarkAndNotifyInitializationComplete() {
  // There is only one real time we need this mutex.  If we get an auth
  // success, and before the initial sync ends we get an auth failure.  In this
  // case we'll be listening to both the AuthWatcher and Syncer, and it's a race
  // between their respective threads to call MarkAndNotify.  We need to make
  // sure the observer is notified once and only once.
  {
    AutoLock lock(initialized_mutex_);
    if (initialized_)
      return;
    initialized_ = true;
  }

  // Notify that initialization is complete.
  if (observer_)
    observer_->OnInitializationComplete();
}

void SyncManager::SyncInternal::Authenticate(const std::string& username,
                                             const std::string& password) {
  DCHECK(username_for_share().empty() ||
        (username == browser_sync::ToUTF8(username_for_share()).get_string()))
        << "Username change from valid username detected";
  if (allstatus()->status().authenticated)
    return;
  if (password.empty()) {
    // TODO(timsteele): Seems like this shouldn't be needed, but auth_watcher
    // currently drops blank password attempts on the floor and doesn't update
    // state; it only LOGs an error in this case. We want to make sure we set
    // our AuthProblem state to denote an error.
    RaiseAuthNeededEvent();
  }
  auth_watcher()->Authenticate(username, password, true);
}

void SyncManager::SyncInternal::AuthenticateForLastKnownUser() {
  std::string username;
  std::string auth_token;
  if (!(auth_watcher()->settings()->GetLastUserAndServiceToken(
        SYNC_SERVICE_NAME, &username, &auth_token))) {
    RaiseAuthNeededEvent();
    return;
  }

  browser_sync::ToPathString s(username);
  if (s.good()) {
    share_.authenticated_name = s.get_string16();
  } else {
    RaiseAuthNeededEvent();
    return;
  }

  // We optimize by opening the directory before the "fresh" authentication
  // attempt completes so that we can immediately begin processing changes.
  if (!dir_manager()->Open(username_for_share())) {
    DCHECK(false) << "Had last known user but could not open directory";
    return;
  }

  // Set the sync data type so that the server only sends us bookmarks
  // changes.
  {
    syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
    if (!lookup.good()) {
      DCHECK(false) << "ScopedDirLookup failed on successfully opened dir";
      return;
    }
    if (lookup->initial_sync_ended())
      MarkAndNotifyInitializationComplete();
  }

  // Load the last-known good auth token into the connection manager and send
  // it off to the AuthWatcher for validation.  The result of the validation
  // will update the connection manager if necessary.
  connection_manager()->set_auth_token(auth_token);
  auth_watcher()->AuthenticateWithToken(username, auth_token);
}

void SyncManager::SyncInternal::RaiseAuthNeededEvent() {
  auth_problem_ = AUTH_PROBLEM_INVALID_GAIA_CREDENTIALS;
  if (observer_)
    observer_->OnAuthProblem(auth_problem_);
}

SyncManager::~SyncManager() {
  delete data_;
}

void SyncManager::SetObserver(Observer* observer) {
  data_->set_observer(observer);
}

void SyncManager::RemoveObserver() {
  data_->set_observer(NULL);
}

void SyncManager::Shutdown() {
  data_->Shutdown();
}

void SyncManager::SyncInternal::Shutdown() {
  // First reset the AuthWatcher in case an auth attempt is in progress so that
  // it terminates gracefully before we shutdown and close other components.
  // Otherwise the attempt can complete after we've closed the directory, for
  // example, and cause initialization to continue, which is bad.
  auth_watcher_->Shutdown();
  auth_watcher_ = NULL;

  if (syncer_thread()) {
    if (!syncer_thread()->Stop(kThreadExitTimeoutMsec))
      DCHECK(false) << "Unable to stop the syncer, it won't be happy...";
  }

  // Shutdown the xmpp buzz connection.
  LOG(INFO) << "P2P: Mediator logout started.";
  if (talk_mediator()) {
    talk_mediator()->Logout();
  }
  LOG(INFO) << "P2P: Mediator logout completed.";

  if (dir_manager()) {
    dir_manager()->FinalSaveChangesForAll();
    dir_manager()->Close(username_for_share());
  }

  // Reset the DirectoryManager and UserSettings so they relinquish sqlite
  // handles to backing files.
  share_.dir_manager.reset();
  user_settings_.reset();

  // We don't want to process any more events.
  dir_change_hookup_.reset();
  syncer_event_.reset();
  authwatcher_hookup_.reset();

#if defined(OS_WIN)
  // Stop the address watch thread by signaling the exit flag.
  // TODO(timsteele): Same as todo in Init().
  SetEvent(address_watch_params_.exit_flag);
#endif

  address_watch_thread_.Stop();

#if defined(OS_WIN)
  CloseHandle(address_watch_params_.exit_flag);
#endif
}

// Listen to model changes, filter out ones initiated by the sync API, and
// saves the rest (hopefully just backend Syncer changes resulting from
// ApplyUpdates) to data_->changelist.
void SyncManager::SyncInternal::HandleChangeEvent(
    const syncable::DirectoryChangeEvent& event) {
  if (event.todo == syncable::DirectoryChangeEvent::TRANSACTION_COMPLETE) {
    HandleTransactionCompleteChangeEvent(event);
    return;
  } else if (event.todo == syncable::DirectoryChangeEvent::CALCULATE_CHANGES) {
    if (event.writer == syncable::SYNCAPI) {
      HandleCalculateChangesChangeEventFromSyncApi(event);
      return;
    }
    HandleCalculateChangesChangeEventFromSyncer(event);
    return;
  } else if (event.todo == syncable::DirectoryChangeEvent::SHUTDOWN) {
    dir_change_hookup_.reset();
  }
}

void SyncManager::SyncInternal::HandleTransactionCompleteChangeEvent(
    const syncable::DirectoryChangeEvent& event) {
  DCHECK_EQ(event.todo, syncable::DirectoryChangeEvent::TRANSACTION_COMPLETE);
  // This notification happens immediately after a syncable WriteTransaction
  // falls out of scope.
  if (change_buffer_.IsEmpty() || !observer_)
    return;

  ReadTransaction trans(GetUserShare());
  vector<ChangeRecord> ordered_changes;
  change_buffer_.GetAllChangesInTreeOrder(&trans, &ordered_changes);
  if (!ordered_changes.empty()) {
    observer_->OnChangesApplied(&trans, &ordered_changes[0],
                                ordered_changes.size());
  }
  change_buffer_.Clear();
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncApi(
    const syncable::DirectoryChangeEvent& event) {
  // We have been notified about a user action changing the bookmark model.
  DCHECK_EQ(event.todo, syncable::DirectoryChangeEvent::CALCULATE_CHANGES);
  DCHECK_EQ(event.writer, syncable::SYNCAPI);
  LOG_IF(WARNING, !change_buffer_.IsEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  bool exists_unsynced_items = false;
  for (syncable::OriginalEntries::const_iterator i = event.originals->begin();
       i != event.originals->end() && !exists_unsynced_items;
       ++i) {
    int64 id = i->ref(syncable::META_HANDLE);
    syncable::Entry e(event.trans, syncable::GET_BY_HANDLE, id);
    DCHECK(e.good());

    if (e.IsRoot()) {
      // Ignore root object, should it ever change.
      continue;
    } else if (!e.Get(syncable::IS_BOOKMARK_OBJECT)) {
      // Ignore non-bookmark objects.
      continue;
    } else if (e.Get(syncable::IS_UNSYNCED)) {
      // Unsynced items will cause us to nudge the the syncer.
      exists_unsynced_items = true;
    }
  }
  if (exists_unsynced_items && syncer_thread()) {
    syncer_thread()->NudgeSyncer(200, SyncerThread::kLocal);  // 1/5 a second.
  }
}

void SyncManager::SyncInternal::HandleCalculateChangesChangeEventFromSyncer(
    const syncable::DirectoryChangeEvent& event) {
  // We only expect one notification per sync step, so change_buffer_ should
  // contain no pending entries.
  DCHECK_EQ(event.todo, syncable::DirectoryChangeEvent::CALCULATE_CHANGES);
  DCHECK_EQ(event.writer, syncable::SYNCER);
  LOG_IF(WARNING, !change_buffer_.IsEmpty()) <<
      "CALCULATE_CHANGES called with unapplied old changes.";

  for (syncable::OriginalEntries::const_iterator i = event.originals->begin();
       i != event.originals->end(); ++i) {
    int64 id = i->ref(syncable::META_HANDLE);
    syncable::Entry e(event.trans, syncable::GET_BY_HANDLE, id);
    bool existed_before = !i->ref(syncable::IS_DEL);
    bool exists_now = e.good() && !e.Get(syncable::IS_DEL);
    DCHECK(e.good());

    // Ignore root object, should it ever change.
    if (e.IsRoot())
      continue;
    // Ignore non-bookmark objects.
    if (!e.Get(syncable::IS_BOOKMARK_OBJECT))
      continue;

    if (exists_now && !existed_before)
      change_buffer_.PushAddedItem(id);
    else if (!exists_now && existed_before)
      change_buffer_.PushDeletedItem(id);
    else if (exists_now && existed_before && BookmarkPropertiesDiffer(*i, e))
      change_buffer_.PushUpdatedItem(id, BookmarkPositionsDiffer(*i, e));
  }
}

SyncManager::Status::Summary
SyncManager::SyncInternal::ComputeAggregatedStatusSummary() {
  switch (allstatus()->status().icon) {
    case AllStatus::OFFLINE:
      return Status::OFFLINE;
    case AllStatus::OFFLINE_UNSYNCED:
      return Status::OFFLINE_UNSYNCED;
    case AllStatus::SYNCING:
      return Status::SYNCING;
    case AllStatus::READY:
      return Status::READY;
    case AllStatus::CONFLICT:
      return Status::CONFLICT;
    case AllStatus::OFFLINE_UNUSABLE:
      return Status::OFFLINE_UNUSABLE;
    default:
      return Status::INVALID;
  }
}

SyncManager::Status SyncManager::SyncInternal::ComputeAggregatedStatus() {
  Status return_status =
      { ComputeAggregatedStatusSummary(),
        allstatus()->status().authenticated,
        allstatus()->status().server_up,
        allstatus()->status().server_reachable,
        allstatus()->status().server_broken,
        allstatus()->status().notifications_enabled,
        allstatus()->status().notifications_received,
        allstatus()->status().notifications_sent,
        allstatus()->status().unsynced_count,
        allstatus()->status().conflicting_count,
        allstatus()->status().syncing,
        allstatus()->status().initial_sync_ended,
        allstatus()->status().syncer_stuck,
        allstatus()->status().updates_available,
        allstatus()->status().updates_received,
        allstatus()->status().disk_full,
        false,   // TODO(ncarter): invalid store?
        allstatus()->status().max_consecutive_errors};
  return return_status;
}

void SyncManager::SyncInternal::HandleSyncerEvent(const SyncerEvent& event) {
  if (!initialized()) {
    // We get here if A) We have successfully authenticated at least once
    // (because we attach HandleSyncerEvent only once we receive notification
    // of successful authentication [locally or otherwise]), but B) the initial
    // sync had not completed at that time.
    if (SyncerStatus(event.last_session).IsShareUsable())
      MarkAndNotifyInitializationComplete();
    return;
  }

  if (!observer_)
    return;

  // Only send an event if this is due to a cycle ending and this cycle
  // concludes a canonical "sync" process; that is, based on what is known
  // locally we are "all happy" and up-to-date.  There may be new changes on
  // the server, but we'll get them on a subsequent sync.
  //
  // Notifications are sent at the end of every sync cycle, regardless of
  // whether we should sync again.
  if (event.what_happened == SyncerEvent::SYNC_CYCLE_ENDED) {
    if (!event.last_session->HasMoreToSync()) {
      observer_->OnSyncCycleCompleted();
    }

    // TODO(chron): Consider changing this back to track HasMoreToSync
    // Only notify peers if a commit has occurred and change the bookmark model.
    if (event.last_session && event.last_session->items_committed()) {
      notification_pending_ = true;
    }

    // SyncCycles are started by the following events: creation of the syncer,
    // (re)connection to buzz, local changes, peer notifications of updates.
    // Peers will be notified of changes made while there is no buzz connection
    // immediately after a connection has been re-established.
    // the next sync cycle.
    // TODO(brg): Move this to TalkMediatorImpl as a SyncerThread event hook.
    if (notification_pending_ && talk_mediator()) {
        LOG(INFO) << "Sending XMPP notification...";
        bool success = talk_mediator()->SendNotification();
        if (success) {
          notification_pending_ = false;
        }
    } else {
      LOG(INFO) << "Didn't send XMPP notification!"
                   << " event.last_session: " << event.last_session
                   << " event.last_session->items_committed(): "
                   << event.last_session->items_committed()
                   << " talk_mediator(): " << talk_mediator();
    }
  }
}

void SyncManager::SyncInternal::HandleAuthWatcherEvent(
    const AuthWatcherEvent& event) {
  // We don't care about an authentication attempt starting event, and we
  // don't want to reset our state to AUTH_PROBLEM_NONE because the fact that
  // an _attempt_ is starting doesn't change the fact that we have an auth
  // problem.
  if (event.what_happened == AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START)
    return;
  // We clear our last auth problem cache on new auth watcher events, and only
  // set it to indicate a problem state for certain AuthWatcherEvent types.
  auth_problem_ = AUTH_PROBLEM_NONE;
  switch (event.what_happened) {
    case AuthWatcherEvent::AUTH_SUCCEEDED:
      // We now know the supplied username and password were valid. If this
      // wasn't the first sync, authenticated_name should already be assigned.
      if (username_for_share().empty()) {
        browser_sync::ToPathString s(event.user_email);
        if (s.good())
          share_.authenticated_name = s.get_string16();
      }

      DCHECK(LowerCaseEqualsASCII(browser_sync::ToUTF8(
          username_for_share()).get_string(),
          StringToLowerASCII(event.user_email).c_str()))
          << "username_for_share= "
          << browser_sync::ToUTF8(username_for_share())
          << ", event.user_email= " << event.user_email;

      if (observer_)
        observer_->OnAuthProblem(AUTH_PROBLEM_NONE);

      // Hook up the DirectoryChangeEvent listener, HandleChangeEvent.
      {
        syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
        if (!lookup.good()) {
          DCHECK(false) << "ScopedDirLookup creation failed; unable to hook "
                        << "up directory change event listener!";
          return;
        }
        dir_change_hookup_.reset(NewEventListenerHookup(
            lookup->changes_channel(), this,
            &SyncInternal::HandleChangeEvent));

        if (lookup->initial_sync_ended())
          MarkAndNotifyInitializationComplete();
      }
      {
        // Start watching the syncer channel directly here.
        DCHECK(syncer_thread() != NULL);
        syncer_event_.reset(NewEventListenerHookup(syncer_thread()->channel(),
            this, &SyncInternal::HandleSyncerEvent));
      }
      return;
    // Authentication failures translate to Status::AuthProblem events.
    case AuthWatcherEvent::GAIA_AUTH_FAILED:     // Invalid GAIA credentials.
    case AuthWatcherEvent::SERVICE_AUTH_FAILED:  // Expired GAIA credentials.
      auth_problem_ = AUTH_PROBLEM_INVALID_GAIA_CREDENTIALS;
      break;
    case AuthWatcherEvent::SERVICE_USER_NOT_SIGNED_UP:
      auth_problem_ = AUTH_PROBLEM_USER_NOT_SIGNED_UP;
      break;
    case AuthWatcherEvent::SERVICE_CONNECTION_FAILED:
      auth_problem_ = AUTH_PROBLEM_CONNECTION_FAILED;
      break;
    default:  // We don't care about the many other AuthWatcherEvent types.
      return;
  }

  // Fire notification that the status changed due to an authentication error.
  if (observer_)
    observer_->OnAuthProblem(auth_problem_);
}

SyncManager::Status::Summary SyncManager::GetStatusSummary() const {
  return data_->ComputeAggregatedStatusSummary();
}

SyncManager::Status SyncManager::GetDetailedStatus() const {
  return data_->ComputeAggregatedStatus();
}

SyncManager::SyncInternal* SyncManager::GetImpl() const { return data_; }

void SyncManager::SaveChanges() {
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

void SyncManager::SetupForTestMode(const sync_char16* test_username) {
  DCHECK(data_) << "SetupForTestMode requires initialization";
  data_->SetupForTestMode(test_username);
}

void SyncManager::SyncInternal::SetupForTestMode(
    const sync_char16* test_username) {
  String16ToPathString(test_username, &share_.authenticated_name);

  if (!dir_manager()->Open(username_for_share()))
    DCHECK(false) << "Could not open directory when running in test mode";

  // Hook up the DirectoryChangeEvent listener, HandleChangeEvent.
  {
    syncable::ScopedDirLookup lookup(dir_manager(), username_for_share());
    if (!lookup.good()) {
      DCHECK(false) << "ScopedDirLookup creation failed; unable to hook "
                    << "up directory change event listener!";
      return;
    }
    dir_change_hookup_.reset(NewEventListenerHookup(
        lookup->changes_channel(), this,
        &SyncInternal::HandleChangeEvent));
  }
  MarkAndNotifyInitializationComplete();
}

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share)
    : lookup_(NULL) {
  DCHECK(share && share->dir_manager.get());
  lookup_ = new syncable::ScopedDirLookup(share->dir_manager.get(),
                                        share->authenticated_name);
  if (!(lookup_->good()))
    DCHECK(false) << "ScopedDirLookup failed on valid DirManager.";
}
BaseTransaction::~BaseTransaction() {
  delete lookup_;
}

UserShare* SyncManager::GetUserShare() const {
  DCHECK(data_->initialized()) << "GetUserShare requires initialization!";
  return data_->GetUserShare();
}

}  // namespace sync_api
