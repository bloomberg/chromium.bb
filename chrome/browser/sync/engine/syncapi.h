// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/syncable/autofill_migration.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class FilePath;

namespace browser_sync {
class JsBackend;
class ModelSafeWorkerRegistrar;

namespace sessions {
struct SyncSessionSnapshot;
}
}

namespace notifier {
struct NotifierOptions;
}

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

namespace sync_pb {
class AppSpecifics;
class AutofillSpecifics;
class AutofillProfileSpecifics;
class BookmarkSpecifics;
class EntitySpecifics;
class ExtensionSpecifics;
class SessionSpecifics;
class NigoriSpecifics;
class PasswordSpecifics;
class PreferenceSpecifics;
class PasswordSpecifics;
class PasswordSpecificsData;
class ThemeSpecifics;
class TypedUrlSpecifics;
}

namespace sync_api {

// Forward declarations of classes to be defined later in this file.
class BaseTransaction;
class HttpPostProviderFactory;
class SyncManager;
class WriteTransaction;

// A UserShare encapsulates the syncable pieces that represent an authenticated
// user and their data (share).
// This encompasses all pieces required to build transaction objects on the
// syncable share.
struct UserShare {
  UserShare();
  ~UserShare();

  // The DirectoryManager itself, which is the parent of Transactions and can
  // be shared across multiple threads (unlike Directory).
  scoped_ptr<syncable::DirectoryManager> dir_manager;

  // The username of the sync user.
  std::string name;
};

// Contains everything needed to talk to and identify a user account.
struct SyncCredentials {
  std::string email;
  std::string sync_token;
};

// A valid BaseNode will never have an ID of zero.
static const int64 kInvalidId = 0;

// BaseNode wraps syncable::Entry, and corresponds to a single object's state.
// This, like syncable::Entry, is intended for use on the stack.  A valid
// transaction is necessary to create a BaseNode or any of its children.
// Unlike syncable::Entry, a sync API BaseNode is identified primarily by its
// int64 metahandle, which we call an ID here.
class BaseNode {
 public:
  // All subclasses of BaseNode must provide a way to initialize themselves by
  // doing an ID lookup.  Returns false on failure.  An invalid or deleted
  // ID will result in failure.
  virtual bool InitByIdLookup(int64 id) = 0;

  // All subclasses of BaseNode must also provide a way to initialize themselves
  // by doing a client tag lookup. Returns false on failure. A deleted node
  // will return FALSE.
  virtual bool InitByClientTagLookup(syncable::ModelType model_type,
      const std::string& tag) = 0;

  // Each object is identified by a 64-bit id (internally, the syncable
  // metahandle).  These ids are strictly local handles.  They will persist
  // on this client, but the same object on a different client may have a
  // different ID value.
  virtual int64 GetId() const;

  // Returns the modification time of the object (in TimeTicks internal format).
  int64 GetModificationTime() const;

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
  std::wstring GetTitle() const;

  // Returns the model type of this object.  The model type is set at node
  // creation time and is expected never to change.
  syncable::ModelType GetModelType() const;

  // Getter specific to the BOOKMARK datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == BOOKMARK.
  const sync_pb::BookmarkSpecifics& GetBookmarkSpecifics() const;

  // Legacy, bookmark-specific getter that wraps GetBookmarkSpecifics() above.
  // Returns the URL of a bookmark object.
  // TODO(ncarter): Remove this datatype-specific accessor.
  GURL GetURL() const;

  // Legacy, bookmark-specific getter that wraps GetBookmarkSpecifics() above.
  // Fill in a vector with the byte data of this node's favicon.  Assumes
  // that the node is a bookmark.
  // Favicons are expected to be PNG images, and though no verification is
  // done on the syncapi client of this, the server may reject favicon updates
  // that are invalid for whatever reason.
  // TODO(ncarter): Remove this datatype-specific accessor.
  void GetFaviconBytes(std::vector<unsigned char>* output) const;

  // Getter specific to the APPS datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == APPS.
  const sync_pb::AppSpecifics& GetAppSpecifics() const;

  // Getter specific to the AUTOFILL datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == AUTOFILL.
  const sync_pb::AutofillSpecifics& GetAutofillSpecifics() const;

  virtual const sync_pb::AutofillProfileSpecifics&
      GetAutofillProfileSpecifics() const;

  // Getter specific to the NIGORI datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == NIGORI.
  const sync_pb::NigoriSpecifics& GetNigoriSpecifics() const;

  // Getter specific to the PASSWORD datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == PASSWORD.
  const sync_pb::PasswordSpecificsData& GetPasswordSpecifics() const;

  // Getter specific to the PREFERENCE datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == PREFERENCE.
  const sync_pb::PreferenceSpecifics& GetPreferenceSpecifics() const;

  // Getter specific to the THEME datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == THEME.
  const sync_pb::ThemeSpecifics& GetThemeSpecifics() const;

  // Getter specific to the TYPED_URLS datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == TYPED_URLS.
  const sync_pb::TypedUrlSpecifics& GetTypedUrlSpecifics() const;

  // Getter specific to the EXTENSIONS datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == EXTENSIONS.
  const sync_pb::ExtensionSpecifics& GetExtensionSpecifics() const;

  // Getter specific to the SESSIONS datatype.  Returns protobuf
  // data.  Can only be called if GetModelType() == SESSIONS.
  const sync_pb::SessionSpecifics& GetSessionSpecifics() const;

  // Returns the local external ID associated with the node.
  int64 GetExternalId() const;

  // Return the ID of the node immediately before this in the sibling order.
  // For the first node in the ordering, return 0.
  int64 GetPredecessorId() const;

  // Return the ID of the node immediately after this in the sibling order.
  // For the last node in the ordering, return 0.
  virtual int64 GetSuccessorId() const;

  // Return the ID of the first child of this node.  If this node has no
  // children, return 0.
  virtual int64 GetFirstChildId() const;

  // These virtual accessors provide access to data members of derived classes.
  virtual const syncable::Entry* GetEntry() const = 0;
  virtual const BaseTransaction* GetTransaction() const = 0;

  // Dumps all node info into a DictionaryValue and returns it.
  // Transfers ownership of the DictionaryValue to the caller.
  DictionaryValue* ToValue() const;

 protected:
  BaseNode();
  virtual ~BaseNode();
  // The server has a size limit on client tags, so we generate a fixed length
  // hash locally. This also ensures that ModelTypes have unique namespaces.
  static std::string GenerateSyncableHash(syncable::ModelType model_type,
      const std::string& client_tag);

  // Determines whether part of the entry is encrypted, and if so attempts to
  // decrypt it. Unless decryption is necessary and fails, this will always
  // return |true|.
  bool DecryptIfNecessary(syncable::Entry* entry);

 private:
  void* operator new(size_t size);  // Node is meant for stack use only.

  // If this node represents a password, this field will hold the actual
  // decrypted password data.
  scoped_ptr<sync_pb::PasswordSpecificsData> password_data_;

  friend class SyncApiTest;
  FRIEND_TEST_ALL_PREFIXES(SyncApiTest, GenerateSyncableHash);

  DISALLOW_COPY_AND_ASSIGN(BaseNode);
};

// WriteNode extends BaseNode to add mutation, and wraps
// syncable::MutableEntry. A WriteTransaction is needed to create a WriteNode.
class WriteNode : public BaseNode {
 public:
  // Create a WriteNode using the given transaction.
  explicit WriteNode(WriteTransaction* transaction);
  virtual ~WriteNode();

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  virtual bool InitByIdLookup(int64 id);
  virtual bool InitByClientTagLookup(syncable::ModelType model_type,
      const std::string& tag);

  // Create a new node with the specified parent and predecessor.  |model_type|
  // dictates the type of the item, and controls which EntitySpecifics proto
  // extension can be used with this item.  Use a NULL |predecessor|
  // to indicate that this is to be the first child.
  // |predecessor| must be a child of |new_parent| or NULL. Returns false on
  // failure.
  bool InitByCreation(syncable::ModelType model_type,
                      const BaseNode& parent,
                      const BaseNode* predecessor);

  // Create nodes using this function if they're unique items that
  // you want to fetch using client_tag. Note that the behavior of these
  // items is slightly different than that of normal items.
  // Most importantly, if it exists locally, this function will
  // actually undelete it
  // Client unique tagged nodes must NOT be folders.
  bool InitUniqueByCreation(syncable::ModelType model_type,
                            const BaseNode& parent,
                            const std::string& client_tag);

  // Each server-created permanent node is tagged with a unique string.
  // Look up the node with the particular tag.  If it does not exist,
  // return false.
  bool InitByTagLookup(const std::string& tag);

  // These Set() functions correspond to the Get() functions of BaseNode.
  void SetIsFolder(bool folder);
  void SetTitle(const std::wstring& title);

  // External ID is a client-only field, so setting it doesn't cause the item to
  // be synced again.
  void SetExternalId(int64 external_id);

  // Remove this node and its children.
  void Remove();

  // Set a new parent and position.  Position is specified by |predecessor|; if
  // it is NULL, the node is moved to the first position.  |predecessor| must
  // be a child of |new_parent| or NULL.  Returns false on failure..
  bool SetPosition(const BaseNode& new_parent, const BaseNode* predecessor);

  // Set the bookmark specifics (url and favicon).
  // Should only be called if GetModelType() == BOOKMARK.
  void SetBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics);

  // Legacy, bookmark-specific setters that wrap SetBookmarkSpecifics() above.
  // Should only be called if GetModelType() == BOOKMARK.
  // TODO(ncarter): Remove these two datatype-specific accessors.
  void SetURL(const GURL& url);
  void SetFaviconBytes(const std::vector<unsigned char>& bytes);

  // Set the app specifics (id, update url, enabled state, etc).
  // Should only be called if GetModelType() == APPS.
  void SetAppSpecifics(const sync_pb::AppSpecifics& specifics);

  // Set the autofill specifics (name and value).
  // Should only be called if GetModelType() == AUTOFILL.
  void SetAutofillSpecifics(const sync_pb::AutofillSpecifics& specifics);

  void SetAutofillProfileSpecifics(
      const sync_pb::AutofillProfileSpecifics& specifics);

  // Set the nigori specifics.
  // Should only be called if GetModelType() == NIGORI.
  void SetNigoriSpecifics(const sync_pb::NigoriSpecifics& specifics);

  // Set the password specifics.
  // Should only be called if GetModelType() == PASSWORD.
  void SetPasswordSpecifics(const sync_pb::PasswordSpecificsData& specifics);

  // Set the preference specifics (name and value).
  // Should only be called if GetModelType() == PREFERENCE.
  void SetPreferenceSpecifics(const sync_pb::PreferenceSpecifics& specifics);

  // Set the theme specifics (name and value).
  // Should only be called if GetModelType() == THEME.
  void SetThemeSpecifics(const sync_pb::ThemeSpecifics& specifics);

  // Set the typed_url specifics (url, title, typed_count, etc).
  // Should only be called if GetModelType() == TYPED_URLS.
  void SetTypedUrlSpecifics(const sync_pb::TypedUrlSpecifics& specifics);

  // Set the extension specifics (id, update url, enabled state, etc).
  // Should only be called if GetModelType() == EXTENSIONS.
  void SetExtensionSpecifics(const sync_pb::ExtensionSpecifics& specifics);

  // Set the session specifics (windows, tabs, navigations etc.).
  // Should only be called if GetModelType() == SESSIONS.
  void SetSessionSpecifics(const sync_pb::SessionSpecifics& specifics);

  // Implementation of BaseNode's abstract virtual accessors.
  virtual const syncable::Entry* GetEntry() const;

  virtual const BaseTransaction* GetTransaction() const;

 private:
  void* operator new(size_t size);  // Node is meant for stack use only.

  // Helper to set model type. This will clear any specifics data.
  void PutModelType(syncable::ModelType model_type);

  // Helper to set the previous node.
  void PutPredecessor(const BaseNode* predecessor);

  // Private helpers to set type-specific protobuf data.  These don't
  // do any checking on the previous modeltype, so they can be used
  // for internal initialization (you can use them to set the modeltype).
  // Additionally, they will mark for syncing if the underlying value
  // changes.
  void PutAppSpecificsAndMarkForSyncing(
      const sync_pb::AppSpecifics& new_value);
  void PutAutofillSpecificsAndMarkForSyncing(
      const sync_pb::AutofillSpecifics& new_value);
  void PutAutofillProfileSpecificsAndMarkForSyncing(
      const sync_pb::AutofillProfileSpecifics& new_value);
  void PutBookmarkSpecificsAndMarkForSyncing(
      const sync_pb::BookmarkSpecifics& new_value);
  void PutNigoriSpecificsAndMarkForSyncing(
      const sync_pb::NigoriSpecifics& new_value);
  void PutPasswordSpecificsAndMarkForSyncing(
      const sync_pb::PasswordSpecifics& new_value);
  void PutPreferenceSpecificsAndMarkForSyncing(
      const sync_pb::PreferenceSpecifics& new_value);
  void PutThemeSpecificsAndMarkForSyncing(
      const sync_pb::ThemeSpecifics& new_value);
  void PutTypedUrlSpecificsAndMarkForSyncing(
      const sync_pb::TypedUrlSpecifics& new_value);
  void PutExtensionSpecificsAndMarkForSyncing(
      const sync_pb::ExtensionSpecifics& new_value);
  void PutSessionSpecificsAndMarkForSyncing(
      const sync_pb::SessionSpecifics& new_value);
  void PutSpecificsAndMarkForSyncing(
      const sync_pb::EntitySpecifics& specifics);

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
class ReadNode : public BaseNode {
 public:
  // Create an unpopulated ReadNode on the given transaction.  Call some flavor
  // of Init to populate the ReadNode with a database entry.
  explicit ReadNode(const BaseTransaction* transaction);
  virtual ~ReadNode();

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  virtual bool InitByIdLookup(int64 id);
  virtual bool InitByClientTagLookup(syncable::ModelType model_type,
      const std::string& tag);

  // There is always a root node, so this can't fail.  The root node is
  // never mutable, so root lookup is only possible on a ReadNode.
  void InitByRootLookup();

  // Each server-created permanent node is tagged with a unique string.
  // Look up the node with the particular tag.  If it does not exist,
  // return false.
  bool InitByTagLookup(const std::string& tag);

  // Implementation of BaseNode's abstract virtual accessors.
  virtual const syncable::Entry* GetEntry() const;
  virtual const BaseTransaction* GetTransaction() const;

 protected:
  ReadNode();

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
class BaseTransaction {
 public:
  // Provide access to the underlying syncable.h objects from BaseNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const = 0;
  const syncable::ScopedDirLookup& GetLookup() const { return *lookup_; }
  browser_sync::Cryptographer* GetCryptographer() const {
    return cryptographer_;
  }

 protected:
  // The ScopedDirLookup is created in the constructor and destroyed
  // in the destructor.  Creation of the ScopedDirLookup is not expected
  // to fail.
  explicit BaseTransaction(UserShare* share);
  virtual ~BaseTransaction();

  BaseTransaction() { lookup_= NULL; }

 private:
  // A syncable ScopedDirLookup, which is the parent of syncable transactions.
  syncable::ScopedDirLookup* lookup_;

  browser_sync::Cryptographer* cryptographer_;

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

// Sync API's ReadTransaction is a read-only BaseTransaction.  It wraps
// a syncable::ReadTransaction.
class ReadTransaction : public BaseTransaction {
 public:
  // Start a new read-only transaction on the specified repository.
  explicit ReadTransaction(UserShare* share);

  // Resume the middle of a transaction. Will not close transaction.
  ReadTransaction(UserShare* share, syncable::BaseTransaction* trans);

  virtual ~ReadTransaction();

  // BaseTransaction override.
  virtual syncable::BaseTransaction* GetWrappedTrans() const;
 private:
  void* operator new(size_t size);  // Transaction is meant for stack use only.

  // The underlying syncable object which this class wraps.
  syncable::BaseTransaction* transaction_;
  bool close_transaction_;

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

// Sync API's WriteTransaction is a read/write BaseTransaction.  It wraps
// a syncable::WriteTransaction.
class WriteTransaction : public BaseTransaction {
 public:
  // Start a new read/write transaction.
  explicit WriteTransaction(UserShare* share);
  virtual ~WriteTransaction();

  // Provide access to the syncable.h transaction from the API WriteNode.
  virtual syncable::BaseTransaction* GetWrappedTrans() const;
  syncable::WriteTransaction* GetWrappedWriteTrans() { return transaction_; }

 protected:
  WriteTransaction() {}

  void SetTransaction(syncable::WriteTransaction* trans) {
      transaction_ = trans;}

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
class SyncManager {
 public:
  // SyncInternal contains the implementation of SyncManager, while abstracting
  // internal types from clients of the interface.
  class SyncInternal;

  // TODO(tim): Depending on how multi-type encryption pans out, maybe we
  // should turn ChangeRecord itself into a class.  Or we could template this
  // wrapper / add a templated method to return unencrypted protobufs.
  class ExtraChangeRecordData {
   public:
    virtual ~ExtraChangeRecordData();

    // Transfers ownership of the DictionaryValue to the caller.
    virtual DictionaryValue* ToValue() const = 0;
  };

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
    ChangeRecord();

    // Transfers ownership of the DictionaryValue to the caller.
    DictionaryValue* ToValue(const BaseTransaction* trans) const;

    int64 id;
    Action action;
    sync_pb::EntitySpecifics specifics;
    linked_ptr<ExtraChangeRecordData> extra;
  };

  // Since PasswordSpecifics is just an encrypted blob, we extend to provide
  // access to unencrypted bits.
  class ExtraPasswordChangeRecordData : public ExtraChangeRecordData {
   public:
    explicit ExtraPasswordChangeRecordData(
        const sync_pb::PasswordSpecificsData& data);
    virtual ~ExtraPasswordChangeRecordData();

    // Transfers ownership of the DictionaryValue to the caller.
    virtual DictionaryValue* ToValue() const;

    const sync_pb::PasswordSpecificsData& unencrypted() const;

   private:
    sync_pb::PasswordSpecificsData unencrypted_;
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

      SUMMARY_STATUS_COUNT,
    };

    Summary summary;
    bool authenticated;      // Successfully authenticated via GAIA.
    bool server_up;          // True if we have received at least one good
                             // reply from the server.
    bool server_reachable;   // True if we received any reply from the server.
    bool server_broken;      // True of the syncer is stopped because of server
                             // issues.
    bool notifications_enabled;  // True only if subscribed for notifications.

    // Notifications counters updated by the actions in synapi.
    int notifications_received;
    int notifications_sent;

    // The max number of consecutive errors from any component.
    int max_consecutive_errors;

    int unsynced_count;

    int conflicting_count;
    bool syncing;
    // True after a client has done a first sync.
    bool initial_sync_ended;
    // True if any syncer is stuck.
    bool syncer_stuck;

    // Total updates available.  If zero, nothing left to download.
    int64 updates_available;
    // Total updates received by the syncer since browser start.
    int updates_received;

    // Of updates_received, how many were tombstones.
    int tombstone_updates_received;
    bool disk_full;
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
    //
    // This will be invoked on the same thread as on which ApplyChanges was
    // called. |changes| is an array of size |change_count|, and contains the
    // ID of each individual item that was changed. |changes| exists only for
    // the duration of the call. If items of multiple data types change at
    // the same time, this method is invoked once per data type and |changes|
    // is restricted to items of the ModelType indicated by |model_type|.
    // Because the observer is passed a |trans|, the observer can assume a
    // read lock on the sync model that will be released after the function
    // returns.
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
    virtual void OnChangesApplied(syncable::ModelType model_type,
                                  const BaseTransaction* trans,
                                  const ChangeRecord* changes,
                                  int change_count) = 0;

    // OnChangesComplete gets called when the TransactionComplete event is
    // posted (after OnChangesApplied finishes), after the transaction lock
    // and the change channel mutex are released.
    //
    // The purpose of this function is to support processors that require
    // split-transactions changes. For example, if a model processor wants to
    // perform blocking I/O due to a change, it should calculate the changes
    // while holding the transaction lock (from within OnChangesApplied), buffer
    // those changes, let the transaction fall out of scope, and then commit
    // those changes from within OnChangesComplete (postponing the blocking
    // I/O to when it no longer holds any lock).
    virtual void OnChangesComplete(syncable::ModelType model_type) = 0;

    // A round-trip sync-cycle took place and the syncer has resolved any
    // conflicts that may have arisen.
    virtual void OnSyncCycleCompleted(
        const browser_sync::sessions::SyncSessionSnapshot* snapshot) = 0;

    // Called when user interaction may be required due to an auth problem.
    virtual void OnAuthError(const GoogleServiceAuthError& auth_error) = 0;

    // Called when a new auth token is provided by the sync server.
    virtual void OnUpdatedToken(const std::string& token) = 0;

    // Called when user interaction is required to obtain a valid passphrase.
    // If the passphrase is required to decrypt something that has
    // already been encrypted (and thus has to match the existing key),
    // |for_decryption| will be true.  If the passphrase is needed for
    // encryption, |for_decryption| will be false.
    virtual void OnPassphraseRequired(bool for_decryption) = 0;

    // Called when the passphrase provided by the user has been accepted and is
    // now used to encrypt sync data.  |bootstrap_token| is an opaque base64
    // encoded representation of the key generated by the accepted passphrase,
    // and is provided to the observer for persistence purposes and use in a
    // future initialization of sync (e.g. after restart).
    virtual void OnPassphraseAccepted(const std::string& bootstrap_token) = 0;

    // Called when initialization is complete to the point that SyncManager can
    // process changes. This does not necessarily mean authentication succeeded
    // or that the SyncManager is online.
    // IMPORTANT: Creating any type of transaction before receiving this
    // notification is illegal!
    // WARNING: Calling methods on the SyncManager before receiving this
    // message, unless otherwise specified, produces undefined behavior.
    virtual void OnInitializationComplete() = 0;

    // The syncer thread has been paused.
    virtual void OnPaused() = 0;

    // The syncer thread has been resumed.
    virtual void OnResumed() = 0;

    // We are no longer permitted to communicate with the server. Sync should
    // be disabled and state cleaned up at once.  This can happen for a number
    // of reasons, e.g. swapping from a test instance to production, or a
    // global stop syncing operation has wiped the store.
    virtual void OnStopSyncingPermanently() = 0;

    // After a request to clear server data, these callbacks are invoked to
    // indicate success or failure
    virtual void OnClearServerDataSucceeded() = 0;
    virtual void OnClearServerDataFailed() = 0;

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
  // |post_factory| will be owned internally and used to create
  // instances of an HttpPostProvider.
  // |model_safe_worker| ownership is given to the SyncManager.
  // |user_agent| is a 7-bit ASCII string suitable for use as the User-Agent
  // HTTP header. Used internally when collecting stats to classify clients.
  // |notifier_options| contains options specific to sync notifications.
  bool Init(const FilePath& database_location,
            const char* sync_server_and_path,
            int sync_server_port,
            bool use_ssl,
            HttpPostProviderFactory* post_factory,
            browser_sync::ModelSafeWorkerRegistrar* registrar,
            const char* user_agent,
            const SyncCredentials& credentials,
            const notifier::NotifierOptions& notifier_options,
            const std::string& restored_key_for_bootstrapping,
            bool setup_for_test_mode);

  // Returns the username last used for a successful authentication.
  // Returns empty if there is no such username.
  const std::string& GetAuthenticatedUsername();

  // Check if the database has been populated with a full "initial" download of
  // sync items for each data type currently present in the routing info.
  // Prerequisite for calling this is that OnInitializationComplete has been
  // called.
  bool InitialSyncEndedForAllEnabledTypes();

  syncable::AutofillMigrationState GetAutofillMigrationState();

  void SetAutofillMigrationState(
    syncable::AutofillMigrationState state);

  syncable::AutofillMigrationDebugInfo GetAutofillMigrationDebugInfo();

  void SetAutofillMigrationDebugInfo(
      syncable::AutofillMigrationDebugInfo::PropertyToSet property_to_set,
      const syncable::AutofillMigrationDebugInfo& info);

  // Migrate tokens from user settings DB to the token service.
  void MigrateTokens();

  // Update tokens that we're using in Sync. Email must stay the same.
  void UpdateCredentials(const SyncCredentials& credentials);

  // Start the SyncerThread.
  void StartSyncing();

  // Attempt to set the passphrase. If the passphrase is valid,
  // OnPassphraseAccepted will be fired to notify the ProfileSyncService and the
  // syncer will be nudged so that any update that was waiting for this
  // passphrase gets applied as soon as possible.
  // If the passphrase in invalid, OnPassphraseRequired will be fired.
  // Calling this metdod again is the appropriate course of action to "retry"
  // with a new passphrase.
  // |is_explicit| is true if the call is in response to the user explicitly
  // setting a passphrase as opposed to implicitly (from the users' perspective)
  // using their Google Account password.  An implicit SetPassphrase will *not*
  // *not* override an explicit passphrase set previously.
  void SetPassphrase(const std::string& passphrase, bool is_explicit);

  // Requests the syncer thread to pause.  The observer's OnPause
  // method will be called when the syncer thread is paused.  Returns
  // false if the syncer thread can not be paused (e.g. if it is not
  // started).
  bool RequestPause();

  // Requests the syncer thread to resume.  The observer's OnResume
  // method will be called when the syncer thread is resumed.  Returns
  // false if the syncer thread can not be resumed (e.g. if it is not
  // paused).
  bool RequestResume();

  // Request a nudge of the syncer, which will cause the syncer thread
  // to run at the next available opportunity.
  void RequestNudge();

  // Request a clearing of all data on the server
  void RequestClearServerData();

  // Adds a listener to be notified of sync events.
  // NOTE: It is OK (in fact, it's probably a good idea) to call this before
  // having received OnInitializationCompleted.
  void SetObserver(Observer* observer);

  // Remove the observer set by SetObserver (no op if none was set).
  // Make sure to call this if the Observer set in SetObserver is being
  // destroyed so the SyncManager doesn't potentially dereference garbage.
  void RemoveObserver();

  // Returns a pointer to the JsBackend (which is owned by the sync
  // manager).  Never returns NULL.  The following events are sent by
  // the returned backend:
  //
  // onSyncNotificationStateChange(boolean notificationsEnabled):
  //   Sent when notifications are enabled or disabled.
  //
  // onSyncIncomingNotification(array changedTypes):
  //   Sent when an incoming notification arrives.  |changedTypes|
  //   contains a list of sync types (strings) which have changed.
  //
  // The following messages are processed by the returned backend:
  //
  // getNotificationState():
  //   If there is a parent router, sends the
  //   onGetNotificationStateFinished(boolean notificationsEnabled)
  //   event to |sender| via the parent router with whether or not
  //   notifications are enabled.
  //
  // getRootNode():
  //   If there is a parent router, sends the
  //   onGetRootNodeFinished(dictionary nodeInfo) event to |sender|
  //   via the parent router with information on the root node.
  //
  // getNodeById(string id):
  //   If there is a parent router, sends the
  //   onGetNodeByIdFinished(dictionary nodeInfo) event to |sender|
  //   via the parent router with information on the node with the
  //   given id (metahandle), if the id is valid and a node with that
  //   id exists.  Otherwise, calls onGetNodeByIdFinished(null).
  //
  // All other messages are dropped.
  browser_sync::JsBackend* GetJsBackend();

  // Status-related getters. Typically GetStatusSummary will suffice, but
  // GetDetailedSyncStatus can be useful for gathering debug-level details of
  // the internals of the sync engine.
  Status::Summary GetStatusSummary() const;
  Status GetDetailedStatus() const;

  // Whether or not the Nigori node is encrypted using an explicit passphrase.
  bool IsUsingExplicitPassphrase();

  // Get the internal implementation for use by BaseTransaction, etc.
  SyncInternal* GetImpl() const;

  // Call periodically from a database-safe thread to persist recent changes
  // to the syncapi model.
  void SaveChanges();

  // Issue a final SaveChanges, close sqlite handles, and stop running threads.
  // Must be called from the same thread that called Init().
  void Shutdown();

  UserShare* GetUserShare() const;

  // Uses a read-only transaction to determine if the directory being synced has
  // any remaining unsynced items.
  bool HasUnsyncedItems() const;

  // Functions used for testing.

  void TriggerOnNotificationStateChangeForTest(
      bool notifications_enabled);

  void TriggerOnIncomingNotificationForTest(
      const syncable::ModelTypeBitSet& model_types);

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

  // Get the value of a header returned in the HTTP response.
  // If the header is not present, returns the empty string.
  virtual const std::string GetResponseHeaderValue(
      const std::string& name) const = 0;

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

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_H_
