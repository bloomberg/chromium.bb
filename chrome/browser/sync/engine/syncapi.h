// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the "sync API", an interface to the syncer
// backend that exposes (1) the core functionality of maintaining a consistent
// local snapshot of a hierarchical object set; (2) a means to transactionally
// access and modify those objects; (3) a means to control client/server
// synchronization tasks, namely: pushing local object modifications to a
// server, pulling nonlocal object modifications from a server to this client,
// and resolving conflicts that may arise between the two; and (4) an
// abstraction of some external functionality that is to be provided by the
// host environment.
//
// This interface is used as the entry point into the syncer backend
// when the backend is compiled as a library and embedded in another
// application.  A goal for this interface layer is to depend on very few
// external types, so that an application can use the sync backend
// without introducing a dependency on specific types.  A non-goal is to
// have binary compatibility across versions or compilers; this allows the
// interface to use C++ classes.  An application wishing to use the sync API
// should ideally compile the syncer backend and this API as part of the
// application's own build, to avoid e.g. mismatches in calling convention,
// structure padding, or name mangling that could arise if there were a
// compiler mismatch.
//
// The schema of the objects in the sync domain is based on the model, which
// is essentially a hierarchy of items and folders similar to a filesystem,
// but with a few important differences.  The sync API contains fields
// such as URL to easily allow the embedding application to store web
// browser bookmarks.  Also, the sync API allows duplicate titles in a parent.
// Consequently, it does not support looking up an object by title
// and parent, since such a lookup is not uniquely determined.  Lastly,
// unlike a filesystem model, objects in the Sync API model have a strict
// ordering within a parent; the position is manipulable by callers, and
// children of a node can be enumerated in the order of their position.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "build/build_config.h"
#include "chrome/browser/google_service_auth_error.h"
#include "googleurl/src/gurl.h"

// The MSVC compiler for Windows requires that any classes exported by, or
// imported from, a dynamic library be marked with an appropriate
// __declspec() decoration. However, we currently use static linkage
// on all platforms.
#define SYNC_EXPORT

// Forward declarations of internal class types so that sync API objects
// may have opaque pointers to these types.
namespace syncable {
class BaseTransaction;
class DirectoryManager;
class Entry;
class MutableEntry;
class ReadTransaction;
class ScopedDirLookup;
class WriteTransaction;
}

namespace sync_api {

// Forward declarations of classes to be defined later in this file.
class BaseTransaction;
class HttpPostProviderFactory;
class ModelSafeWorkerInterface;
class SyncManager;
class WriteTransaction;
struct UserShare;

// A valid BaseNode will never have an ID of zero.
static const int64 kInvalidId = 0;

// BaseNode wraps syncable::Entry, and corresponds to a single object's state.
// This, like syncable::Entry, is intended for use on the stack.  A valid
// transaction is necessary to create a BaseNode or any of its children.
// Unlike syncable::Entry, a sync API BaseNode is identified primarily by its
// int64 metahandle, which we call an ID here.
class SYNC_EXPORT BaseNode {
 public:
  // All subclasses of BaseNode must provide a way to initialize themselves by
  // doing an ID lookup.  Returns false on failure.  An invalid or deleted
  // ID will result in failure.
  virtual bool InitByIdLookup(int64 id) = 0;

  // Each object is identified by a 64-bit id (internally, the syncable
  // metahandle).  These ids are strictly local handles.  They will persist
  // on this client, but the same object on a different client may have a
  // different ID value.
  int64 GetId() const;

  // Nodes are hierarchically arranged into a single-rooted tree.
  // InitByRootLookup on ReadNode allows access to the root. GetParentId is
  // how you find a node's parent.
  int64 GetParentId() const;

  // Nodes are either folders or not.  This corresponds to the IS_DIR property
  // of syncable::Entry.
  bool GetIsFolder() const;

  // Returns the title of the object.
  // Uniqueness of the title is not enforced on siblings -- it is not an error
  // for two children to share a title.
  const std::wstring& GetTitle() const;

  // Returns the URL of a bookmark object.
  const GURL& GetURL() const;

  // Return a pointer to the byte data of the favicon image for this node.
  // Will return NULL if there is no favicon data associated with this node.
  // The length of the array is returned to the caller via |size_in_bytes|.
  // Favicons are expected to be PNG images, and though no verification is
  // done on the syncapi client of this, the server may reject favicon updates
  // that are invalid for whatever reason.
  const unsigned char* GetFaviconBytes(size_t* size_in_bytes);

  // Returns the local external ID associated with the node.
  int64 GetExternalId() const;

  // Return the ID of the node immediately before this in the sibling order.
  // For the first node in the ordering, return 0.
  int64 GetPredecessorId() const;

  // Return the ID of the node immediately after this in the sibling order.
  // For the last node in the ordering, return 0.
  int64 GetSuccessorId() const;

  // Return the ID of the first child of this node.  If this node has no
  // children, return 0.
  int64 GetFirstChildId() const;

  // Get an array containing the IDs of this node's children.  The memory is
  // owned by BaseNode and becomes invalid if GetChildIds() is called a second
  // time on this node, or when the node is destroyed.  Return the array size
  // in the child_count parameter.
  const int64* GetChildIds(size_t* child_count) const;

  // These virtual accessors provide access to data members of derived classes.
  virtual const syncable::Entry* GetEntry() const = 0;
  virtual const BaseTransaction* GetTransaction() const = 0;

 protected:
  BaseNode();
  virtual ~BaseNode();

 private:
  struct BaseNodeInternal;

  // Node is meant for stack use only.
  void* operator new(size_t size);

  // Provides storage for member functions that return pointers to class
  // memory, e.g. C strings returned by GetTitle().
  BaseNodeInternal* data_;

  DISALLOW_COPY_AND_ASSIGN(BaseNode);
};

// WriteNode extends BaseNode to add mutation, and wraps
// syncable::MutableEntry. A WriteTransaction is needed to create a WriteNode.
class SYNC_EXPORT WriteNode : public BaseNode {
 public:
  // Create a WriteNode using the given transaction.
  explicit WriteNode(WriteTransaction* transaction);
  virtual ~WriteNode();

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  virtual bool InitByIdLookup(int64 id);

  // Create a new node with the specified parent and predecessor.  Use a NULL
  // |predecessor| to indicate that this is to be the first child.
  // |predecessor| must be a child of |new_parent| or NULL. Returns false on
  // failure.
  bool InitByCreation(const BaseNode& parent, const BaseNode* predecessor);

  // These Set() functions correspond to the Get() functions of BaseNode.
  void SetIsFolder(bool folder);
  void SetTitle(const std::wstring& title);
  void SetURL(const GURL& url);
  void SetFaviconBytes(const unsigned char* bytes, size_t size_in_bytes);
  // External ID is a client-only field, so setting it doesn't cause the item to
  // be synced again.
  void SetExternalId(int64 external_id);

  // Remove this node and its children.
  void Remove();

  // Set a new parent and position.  Position is specified by |predecessor|; if
  // it is NULL, the node is moved to the first position.  |predecessor| must
  // be a child of |new_parent| or NULL.  Returns false on failure..
  bool SetPosition(const BaseNode& new_parent, const BaseNode* predecessor);

  // Implementation of BaseNode's abstract virtual accessors.
  virtual const syncable::Entry* GetEntry() const;

  virtual const BaseTransaction* GetTransaction() const;

 private:
  void* operator new(size_t size);  // Node is meant for stack use only.

  // Helper to set the previous node.
  void PutPredecessor(const BaseNode* predecessor);

  // Sets IS_UNSYNCED and SYNCING to ensure this entry is considered in an
  // upcoming commit pass.
  void MarkForSyncing();

  // The underlying syncable object which this class wraps.
  syncable::MutableEntry* entry_;

  // The sync API transaction that is the parent of this node.
  WriteTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(WriteNode);
};

// ReadNode wraps a syncable::Entry to provide the functionality of a
// read-only BaseNode.
class SYNC_EXPORT ReadNode : public BaseNode {
 public:
  // Create an unpopulated ReadNode on the given transaction.  Call some flavor
  // of Init to populate the ReadNode with a database entry.
  explicit ReadNode(const BaseTransaction* transaction);
  virtual ~ReadNode();

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  virtual bool InitByIdLookup(int64 id);

  // There is always a root node, so this can't fail.  The root node is
  // never mutable, so root lookup is only possible on a ReadNode.
  void InitByRootLookup();

  // Each server-created permanent node is tagged with a unique string.
  // Look up the node with the particular tag.  If it does not exist,
  // return false.  Since these nodes are special, lookup is only
  // provided through ReadNode.
  bool InitByTagLookup(const std::string& tag);

  // Implementation of BaseNode's abstract virtual accessors.
  virtual const syncable::Entry* GetEntry() const;
  virtual const BaseTransaction* GetTransaction() const;

 private:
  void* operator new(size_t size);  // Node is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::Entry* entry_;

  // The sync API transaction that is the parent of this node.
  const BaseTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadNode);
};

// Sync API's BaseTransaction, ReadTransaction, and WriteTransaction allow for
// batching of several read and/or write operations.  The read and write
// operations are performed by creating ReadNode and WriteNode instances using
// the transaction. These transaction classes wrap identically named classes in
// syncable, and are used in a similar way. Unlike syncable::BaseTransaction,
// whose construction requires an explicit syncable::ScopedDirLookup, a sync
// API BaseTransaction creates its own ScopedDirLookup implicitly.
class SYNC_EXPORT BaseTransaction {
 public:
  // Provide access to the underlying syncable.h objects from BaseNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const = 0;
  const syncable::ScopedDirLookup& GetLookup() const { return *lookup_; }

 protected:
  // The ScopedDirLookup is created in the constructor and destroyed
  // in the destructor.  Creation of the ScopedDirLookup is not expected
  // to fail.
  explicit BaseTransaction(UserShare* share);
  virtual ~BaseTransaction();

 private:
  // A syncable ScopedDirLookup, which is the parent of syncable transactions.
  syncable::ScopedDirLookup* lookup_;

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

// Sync API's ReadTransaction is a read-only BaseTransaction.  It wraps
// a syncable::ReadTransaction.
class SYNC_EXPORT ReadTransaction : public BaseTransaction {
 public:
  // Start a new read-only transaction on the specified repository.
  explicit ReadTransaction(UserShare* share);
  virtual ~ReadTransaction();

  // BaseTransaction override.
  virtual syncable::BaseTransaction* GetWrappedTrans() const;
 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::ReadTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

// Sync API's WriteTransaction is a read/write BaseTransaction.  It wraps
// a syncable::WriteTransaction.
class SYNC_EXPORT WriteTransaction : public BaseTransaction {
 public:
  // Start a new read/write transaction.
  explicit WriteTransaction(UserShare* share);
  virtual ~WriteTransaction();

  // Provide access to the syncable.h transaction from the API WriteNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const;
  syncable::WriteTransaction* GetWrappedWriteTrans() { return transaction_; }

 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::WriteTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(WriteTransaction);
};

// SyncManager encapsulates syncable::DirectoryManager and serves as the parent
// of all other objects in the sync API.  SyncManager is thread-safe.  If
// multiple threads interact with the same local sync repository (i.e. the
// same sqlite database), they should share a single SyncManager instance.  The
// caller should typically create one SyncManager for the lifetime of a user
// session.
class SYNC_EXPORT SyncManager {
 public:
  // SyncInternal contains the implementation of SyncManager, while abstracting
  // internal types from clients of the interface.
  class SyncInternal;

  // ChangeRecord indicates a single item that changed as a result of a sync
  // operation.  This gives the sync id of the node that changed, and the type
  // of change.  To get the actual property values after an ADD or UPDATE, the
  // client should get the node with InitByIdLookup(), using the provided id.
  struct ChangeRecord {
    enum Action {
      ACTION_ADD,
      ACTION_DELETE,
      ACTION_UPDATE,
    };
    ChangeRecord() : id(kInvalidId), action(ACTION_ADD) {}
    int64 id;
    Action action;
  };

  // Status encapsulates detailed state about the internals of the SyncManager.
  struct Status {
    // Summary is a distilled set of important information that the end-user may
    // wish to be informed about (through UI, for example). Note that if a
    // summary state requires user interaction (such as auth failures), more
    // detailed information may be contained in additional status fields.
    enum Summary {
      // The internal instance is in an unrecognizable state. This should not
      // happen.
      INVALID = 0,
      // Can't connect to server, but there are no pending changes in
      // our local cache.
      OFFLINE,
      // Can't connect to server, and there are pending changes in our
      // local cache.
      OFFLINE_UNSYNCED,
      // Connected and syncing.
      SYNCING,
      // Connected, no pending changes.
      READY,
      // Internal sync error.
      CONFLICT,
      // Can't connect to server, and we haven't completed the initial
      // sync yet.  So there's nothing we can do but wait for the server.
      OFFLINE_UNUSABLE,
    };
    Summary summary;

    // Various server related information.
    bool authenticated;      // Successfully authenticated via GAIA.
    bool server_up;          // True if we have received at least one good
                             // reply from the server.
    bool server_reachable;   // True if we received any reply from the server.
    bool server_broken;      // True of the syncer is stopped because of server
                             // issues.

    bool notifications_enabled;  // True only if subscribed for notifications.
    int notifications_received;
    int notifications_sent;

    // Various Syncer data.
    int unsynced_count;
    int conflicting_count;
    bool syncing;
    bool initial_sync_ended;
    bool syncer_stuck;
    int64 updates_available;
    int64 updates_received;
    bool disk_full;
    bool invalid_store;
    int max_consecutive_errors;  // The max number of errors from any component.
  };

  // An interface the embedding application implements to receive notifications
  // from the SyncManager.  Register an observer via SyncManager::AddObserver.
  // This observer is an event driven model as the events may be raised from
  // different internal threads, and simply providing an "OnStatusChanged" type
  // notification complicates things such as trying to determine "what changed",
  // if different members of the Status object are modified from different
  // threads.  This way, the event is explicit, and it is safe for the Observer
  // to dispatch to a native thread or synchronize accordingly.
  class Observer {
   public:
    Observer() { }
    virtual ~Observer() { }
    // Notify the observer that changes have been applied to the sync model.
    // This will be invoked on the same thread as on which ApplyChanges was
    // called. |changes| is an array of size |change_count|, and contains the ID
    // of each individual item that was changed.  |changes| exists only
    // for the duration of the call.  Because the observer is passed a |trans|,
    // the observer can assume a read lock on the database that will be released
    // after the function returns.
    //
    // The SyncManager constructs |changes| in the following guaranteed order:
    //
    // 1. Deletions, from leaves up to parents.
    // 2. Updates to existing items with synced parents & predecessors.
    // 3. New items with synced parents & predecessors.
    // 4. Items with parents & predecessors in |changes|.
    // 5. Repeat #4 until all items are in |changes|.
    //
    // Thus, an implementation of OnChangesApplied should be able to
    // process the change records in the order without having to worry about
    // forward dependencies.  But since deletions come before reparent
    // operations, a delete may temporarily orphan a node that is
    // updated later in the list.
    virtual void OnChangesApplied(const BaseTransaction* trans,
                                  const ChangeRecord* changes,
                                  int change_count) = 0;

    // A round-trip sync-cycle took place and the syncer has resolved any
    // conflicts that may have arisen.  This is kept separate from
    // OnStatusChanged as there isn't really any state update; it is plainly
    // a notification of a state transition.
    virtual void OnSyncCycleCompleted() = 0;

    // Called when user interaction may be required due to an auth problem.
    virtual void OnAuthError(const GoogleServiceAuthError& auth_error) = 0;

    // Called when initialization is complete to the point that SyncManager can
    // process changes. This does not necessarily mean authentication succeeded
    // or that the SyncManager is online.
    // IMPORTANT: Creating any type of transaction before receiving this
    // notification is illegal!
    // WARNING: Calling methods on the SyncManager before receiving this
    // message, unless otherwise specified, produces undefined behavior.
    virtual void OnInitializationComplete() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Create an uninitialized SyncManager.  Callers must Init() before using.
  SyncManager();
  virtual ~SyncManager();

  // Initialize the sync manager.  |database_location| specifies the path of
  // the directory in which to locate a sqlite repository storing the syncer
  // backend state. Initialization will open the database, or create it if it
  // does not already exist. Returns false on failure.
  // |sync_server_and_path| and |sync_server_port| represent the Chrome sync
  // server to use, and |use_ssl| specifies whether to communicate securely;
  // the default is false.
  // |gaia_service_id| is the service id used for GAIA authentication. If it's
  // null then default will be used.
  // |post_factory| will be owned internally and used to create
  // instances of an HttpPostProvider.
  // |auth_post_factory| will be owned internally and used to create
  // instances of an HttpPostProvider for communicating with GAIA.
  // TODO(timsteele): It seems like one factory should suffice, but for now to
  // avoid having to deal with threading issues since the auth code and syncer
  // code live on separate threads that run simultaneously, we just dedicate
  // one to each component. Long term we may want to reconsider the HttpBridge
  // API to take all the params in one chunk in a threadsafe manner.. which is
  // still suboptimal as there will be high contention between the two threads
  // on startup; so maybe what we have now is the best solution- it does mirror
  // the CURL implementation as each thread creates their own internet handle.
  // Investigate.
  // |model_safe_worker| ownership is given to the SyncManager.
  // |user_agent| is a 7-bit ASCII string suitable for use as the User-Agent
  // HTTP header. Used internally when collecting stats to classify clients.
  bool Init(const FilePath& database_location,
            const char* sync_server_and_path,
            int sync_server_port,
            const char* gaia_service_id,
            const char* gaia_source,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            HttpPostProviderFactory* auth_post_factory,
            ModelSafeWorkerInterface* model_safe_worker,
            bool attempt_last_user_authentication,
            const char* user_agent);

  // Returns the username last used for a successful authentication.
  // Returns empty if there is no such username.
  const std::string& GetAuthenticatedUsername();

  // Submit credentials to GAIA for verification and start the
  // syncing process on success. On success, both |username| and the obtained
  // auth token are persisted on disk for future re-use.
  // If authentication fails, OnAuthProblem is called on our Observer.
  // The Observer may, in turn, decide to try again with new
  // credentials. Calling this method again is the appropriate course of action
  // to "retry".
  // |username| and |password| are expected to be owned by the caller.
  void Authenticate(const char* username, const char* password);

  // Adds a listener to be notified of sync events.
  // NOTE: It is OK (in fact, it's probably a good idea) to call this before
  // having received OnInitializationCompleted.
  void SetObserver(Observer* observer);

  // Remove the observer set by SetObserver (no op if none was set).
  // Make sure to call this if the Observer set in SetObserver is being
  // destroyed so the SyncManager doesn't potentially dereference garbage.
  void RemoveObserver();

  // Status-related getters. Typically GetStatusSummary will suffice, but
  // GetDetailedSyncStatus can be useful for gathering debug-level details of
  // the internals of the sync engine.
  Status::Summary GetStatusSummary() const;
  Status GetDetailedStatus() const;

  // Get the internal implementation for use by BaseTransaction, etc.
  SyncInternal* GetImpl() const;

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // Invoking this method will result in the syncapi bypassing authentication
  // and opening a local store suitable for testing client code. When in this
  // mode, nothing will ever get synced to a server (in fact no HTTP
  // communication will take place).
  // Note: The SyncManager precondition that you must first call Init holds;
  // this will fail unless we're initialized.
  void SetupForTestMode(const std::wstring& test_username);

  // Issue a final SaveChanges, close sqlite handles, and stop running threads.
  // Must be called from the same thread that called Init().
  void Shutdown();

  UserShare* GetUserShare() const;

 private:
  // An opaque pointer to the nested private class.
  SyncInternal* data_;

  DISALLOW_COPY_AND_ASSIGN(SyncManager);
};

// An interface the embedding application (e.g. Chromium) implements to
// provide required HTTP POST functionality to the syncer backend.
// This interface is designed for one-time use. You create one, use it, and
// create another if you want to make a subsequent POST.
// TODO(timsteele): Bug 1482576. Consider splitting syncapi.h into two files:
// one for the API defining the exports, which doesn't need to be included from
// anywhere internally, and another file for the interfaces like this one.
class HttpPostProviderInterface {
 public:
  HttpPostProviderInterface() { }
  virtual ~HttpPostProviderInterface() { }

  // Use specified user agent string when POSTing. If not called a default UA
  // may be used.
  virtual void SetUserAgent(const char* user_agent) = 0;

  // Add additional headers to the request.
  virtual void SetExtraRequestHeaders(const char * headers) = 0;

  // Set the URL to POST to.
  virtual void SetURL(const char* url, int port) = 0;

  // Set the type, length and content of the POST payload.
  // |content_type| is a null-terminated MIME type specifier.
  // |content| is a data buffer; Do not interpret as a null-terminated string.
  // |content_length| is the total number of chars in |content|. It is used to
  // assign/copy |content| data.
  virtual void SetPostPayload(const char* content_type, int content_length,
                              const char* content) = 0;

  // Returns true if the URL request succeeded. If the request failed,
  // os_error() may be non-zero and hence contain more information.
  virtual bool MakeSynchronousPost(int* os_error_code, int* response_code) = 0;

  // Get the length of the content returned in the HTTP response.
  // This does not count the trailing null-terminating character returned
  // by GetResponseContent, so it is analogous to calling string.length.
  virtual int GetResponseContentLength() const = 0;

  // Get the content returned in the HTTP response.
  // This is a null terminated string of characters.
  // Value should be copied.
  virtual const char* GetResponseContent() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpPostProviderInterface);
};

// A factory to create HttpPostProviders to hide details about the
// implementations and dependencies.
// A factory instance itself should be owned by whomever uses it to create
// HttpPostProviders.
class HttpPostProviderFactory {
 public:
  // Obtain a new HttpPostProviderInterface instance, owned by caller.
  virtual HttpPostProviderInterface* Create() = 0;

  // When the interface is no longer needed (ready to be cleaned up), clients
  // must call Destroy().
  // This allows actual HttpPostProvider subclass implementations to be
  // reference counted, which is useful if a particular implementation uses
  // multiple threads to serve network requests.
  virtual void Destroy(HttpPostProviderInterface* http) = 0;
  virtual ~HttpPostProviderFactory() { }
};

// A class syncapi clients should use whenever the underlying model is bound to
// a particular thread in the embedding application. This exposes an interface
// by which any model-modifying invocations will be forwarded to the
// appropriate thread in the embedding application.
// "model safe" refers to not allowing an embedding application model to fall
// out of sync with the syncable::Directory due to race conditions.
class ModelSafeWorkerInterface {
 public:
  virtual ~ModelSafeWorkerInterface() { }
  // A Visitor is passed to CallDoWorkFromModelSafeThreadAndWait invocations,
  // and it's sole purpose is to provide a way for the ModelSafeWorkerInterface
  // implementation to actually _do_ the work required, by calling the only
  // method on this class, DoWork().
  class Visitor {
   public:
    virtual ~Visitor() { }
    // When on a model safe thread, this should be called to have the syncapi
    // actually perform the work needing to be done.
    virtual void DoWork() = 0;
  };
  // Subclasses should implement to invoke DoWork on |visitor| once on a thread
  // appropriate for data model modifications.
  // While it doesn't hurt, the impl does not need to be re-entrant (for now).
  // Note: |visitor| is owned by caller.
  virtual void CallDoWorkFromModelSafeThreadAndWait(Visitor* visitor) = 0;
};

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_H_
