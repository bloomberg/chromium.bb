// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_H_
#pragma once

#include <algorithm>
#include <bitset>
#include <iosfwd>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/autofill_migration.h"
#include "chrome/browser/sync/syncable/blob.h"
#include "chrome/browser/sync/syncable/dir_open_result.h"
#include "chrome/browser/sync/syncable/directory_event.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/channel.h"
#include "chrome/browser/sync/util/dbgq.h"
#include "chrome/common/deprecated/event_sys.h"

struct PurgeInfo;

namespace sync_api {
class ReadTransaction;
class WriteNode;
class ReadNode;
}

namespace syncable {
class Entry;

std::ostream& operator<<(std::ostream& s, const Entry& e);

class DirectoryBackingStore;

static const int64 kInvalidMetaHandle = 0;

enum {
  BEGIN_FIELDS = 0,
  INT64_FIELDS_BEGIN = BEGIN_FIELDS
};

enum MetahandleField {
  // Primary key into the table.  Keep this as a handle to the meta entry
  // across transactions.
  META_HANDLE = INT64_FIELDS_BEGIN
};

enum BaseVersion {
  // After initial upload, the version is controlled by the server, and is
  // increased whenever the data or metadata changes on the server.
  BASE_VERSION = META_HANDLE + 1,
};

enum Int64Field {
  SERVER_VERSION = BASE_VERSION + 1,
  MTIME,
  SERVER_MTIME,
  CTIME,
  SERVER_CTIME,

  // A numeric position value that indicates the relative ordering of
  // this object among its siblings.
  SERVER_POSITION_IN_PARENT,

  LOCAL_EXTERNAL_ID,  // ID of an item in the external local storage that this
                      // entry is associated with. (such as bookmarks.js)

  INT64_FIELDS_END
};

enum {
  INT64_FIELDS_COUNT = INT64_FIELDS_END,
  ID_FIELDS_BEGIN = INT64_FIELDS_END,
};

enum IdField {
  // Code in InitializeTables relies on ID being the first IdField value.
  ID = ID_FIELDS_BEGIN,
  PARENT_ID,
  SERVER_PARENT_ID,

  PREV_ID,
  NEXT_ID,
  ID_FIELDS_END
};

enum {
  ID_FIELDS_COUNT = ID_FIELDS_END - ID_FIELDS_BEGIN,
  BIT_FIELDS_BEGIN = ID_FIELDS_END
};

enum IndexedBitField {
  IS_UNSYNCED = BIT_FIELDS_BEGIN,
  IS_UNAPPLIED_UPDATE,
  INDEXED_BIT_FIELDS_END,
};

enum IsDelField {
  IS_DEL = INDEXED_BIT_FIELDS_END,
};

enum BitField {
  IS_DIR = IS_DEL + 1,
  SERVER_IS_DIR,
  SERVER_IS_DEL,
  BIT_FIELDS_END
};

enum {
  BIT_FIELDS_COUNT = BIT_FIELDS_END - BIT_FIELDS_BEGIN,
  STRING_FIELDS_BEGIN = BIT_FIELDS_END
};

enum StringField {
  // Name, will be truncated by server. Can be duplicated in a folder.
  NON_UNIQUE_NAME = STRING_FIELDS_BEGIN,
  // The server version of |NON_UNIQUE_NAME|.
  SERVER_NON_UNIQUE_NAME,

  // A tag string which identifies this node as a particular top-level
  // permanent object.  The tag can be thought of as a unique key that
  // identifies a singleton instance.
  UNIQUE_SERVER_TAG,  // Tagged by the server
  UNIQUE_CLIENT_TAG,  // Tagged by the client
  STRING_FIELDS_END,
};

enum {
  STRING_FIELDS_COUNT = STRING_FIELDS_END - STRING_FIELDS_BEGIN,
  PROTO_FIELDS_BEGIN = STRING_FIELDS_END
};

// From looking at the sqlite3 docs, it's not directly stated, but it
// seems the overhead for storing a NULL blob is very small.
enum ProtoField {
  SPECIFICS = PROTO_FIELDS_BEGIN,
  SERVER_SPECIFICS,
  PROTO_FIELDS_END,
};

enum {
  PROTO_FIELDS_COUNT = PROTO_FIELDS_END - PROTO_FIELDS_BEGIN
};

enum {
  FIELD_COUNT = PROTO_FIELDS_END,
  // Past this point we have temporaries, stored in memory only.
  BEGIN_TEMPS = PROTO_FIELDS_END,
  BIT_TEMPS_BEGIN = BEGIN_TEMPS,
};

enum BitTemp {
  SYNCING = BIT_TEMPS_BEGIN,
  BIT_TEMPS_END,
};

enum {
  BIT_TEMPS_COUNT = BIT_TEMPS_END - BIT_TEMPS_BEGIN
};

class BaseTransaction;
class WriteTransaction;
class ReadTransaction;
class Directory;
class ScopedDirLookup;

// Instead of:
//   Entry e = transaction.GetById(id);
// use:
//   Entry e(transaction, GET_BY_ID, id);
//
// Why?  The former would require a copy constructor, and it would be difficult
// to enforce that an entry never outlived its transaction if there were a copy
// constructor.
enum GetById {
  GET_BY_ID
};

enum GetByClientTag {
  GET_BY_CLIENT_TAG
};

enum GetByServerTag {
  GET_BY_SERVER_TAG
};

enum GetByHandle {
  GET_BY_HANDLE
};

enum Create {
  CREATE
};

enum CreateNewUpdateItem {
  CREATE_NEW_UPDATE_ITEM
};

typedef std::set<int64> MetahandleSet;

// Why the singular enums?  So the code compile-time dispatches instead of
// runtime dispatches as it would with a single enum and an if() statement.

// The EntryKernel class contains the actual data for an entry.
struct EntryKernel {
 private:
  std::string string_fields[STRING_FIELDS_COUNT];
  sync_pb::EntitySpecifics specifics_fields[PROTO_FIELDS_COUNT];
  int64 int64_fields[INT64_FIELDS_COUNT];
  Id id_fields[ID_FIELDS_COUNT];
  std::bitset<BIT_FIELDS_COUNT> bit_fields;
  std::bitset<BIT_TEMPS_COUNT> bit_temps;

 public:
  EntryKernel();
  ~EntryKernel();

  // Set the dirty bit, and optionally add this entry's metahandle to
  // a provided index on dirty bits in |dirty_index|. Parameter may be null,
  // and will result only in setting the dirty bit of this entry.
  inline void mark_dirty(syncable::MetahandleSet* dirty_index) {
    if (!dirty_ && dirty_index) {
      DCHECK_NE(0, ref(META_HANDLE));
      dirty_index->insert(ref(META_HANDLE));
    }
    dirty_ = true;
  }

  // Clear the dirty bit, and optionally remove this entry's metahandle from
  // a provided index on dirty bits in |dirty_index|. Parameter may be null,
  // and will result only in clearing dirty bit of this entry.
  inline void clear_dirty(syncable::MetahandleSet* dirty_index) {
    if (dirty_ && dirty_index) {
      DCHECK_NE(0, ref(META_HANDLE));
      dirty_index->erase(ref(META_HANDLE));
    }
    dirty_ = false;
  }

  inline bool is_dirty() const {
    return dirty_;
  }

  // Setters.
  inline void put(MetahandleField field, int64 value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(Int64Field field, int64 value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(IdField field, const Id& value) {
    id_fields[field - ID_FIELDS_BEGIN] = value;
  }
  inline void put(BaseVersion field, int64 value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(IndexedBitField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(IsDelField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(BitField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(StringField field, const std::string& value) {
    string_fields[field - STRING_FIELDS_BEGIN] = value;
  }
  inline void put(ProtoField field, const sync_pb::EntitySpecifics& value) {
    specifics_fields[field - PROTO_FIELDS_BEGIN].CopyFrom(value);
  }
  inline void put(BitTemp field, bool value) {
    bit_temps[field - BIT_TEMPS_BEGIN] = value;
  }

  // Const ref getters.
  inline int64 ref(MetahandleField field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline int64 ref(Int64Field field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline const Id& ref(IdField field) const {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline int64 ref(BaseVersion field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline bool ref(IndexedBitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(IsDelField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(BitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline const std::string& ref(StringField field) const {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline const sync_pb::EntitySpecifics& ref(ProtoField field) const {
    return specifics_fields[field - PROTO_FIELDS_BEGIN];
  }
  inline bool ref(BitTemp field) const {
    return bit_temps[field - BIT_TEMPS_BEGIN];
  }

  // Non-const, mutable ref getters for object types only.
  inline std::string& mutable_ref(StringField field) {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline sync_pb::EntitySpecifics& mutable_ref(ProtoField field) {
    return specifics_fields[field - PROTO_FIELDS_BEGIN];
  }
  inline Id& mutable_ref(IdField field) {
    return id_fields[field - ID_FIELDS_BEGIN];
  }

 private:
  // Tracks whether this entry needs to be saved to the database.
  bool dirty_;
};

// A read-only meta entry.
class Entry {
  friend class Directory;
  friend std::ostream& operator << (std::ostream& s, const Entry& e);

 public:
  // After constructing, you must check good() to test whether the Get
  // succeeded.
  Entry(BaseTransaction* trans, GetByHandle, int64 handle);
  Entry(BaseTransaction* trans, GetById, const Id& id);
  Entry(BaseTransaction* trans, GetByServerTag, const std::string& tag);
  Entry(BaseTransaction* trans, GetByClientTag, const std::string& tag);

  bool good() const { return 0 != kernel_; }

  BaseTransaction* trans() const { return basetrans_; }

  // Field accessors.
  inline int64 Get(MetahandleField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline Id Get(IdField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline int64 Get(Int64Field field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline int64 Get(BaseVersion field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(IndexedBitField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(IsDelField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(BitField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  const std::string& Get(StringField field) const;
  inline const sync_pb::EntitySpecifics& Get(ProtoField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(BitTemp field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }

  ModelType GetServerModelType() const;
  ModelType GetModelType() const;

  // If this returns false, we shouldn't bother maintaining
  // a position value (sibling ordering) for this item.
  bool ShouldMaintainPosition() const {
    return GetModelType() == BOOKMARKS;
  }

  inline bool ExistsOnClientBecauseNameIsNonEmpty() const {
    DCHECK(kernel_);
    return !kernel_->ref(NON_UNIQUE_NAME).empty();
  }

  inline bool IsRoot() const {
    DCHECK(kernel_);
    return kernel_->ref(ID).IsRoot();
  }

  Directory* dir() const;

  const EntryKernel GetKernelCopy() const {
    return *kernel_;
  }

 protected:  // Don't allow creation on heap, except by sync API wrappers.
  friend class sync_api::ReadNode;
  void* operator new(size_t size) { return (::operator new)(size); }

  inline Entry(BaseTransaction* trans)
      : basetrans_(trans),
        kernel_(NULL) { }

 protected:

  BaseTransaction* const basetrans_;

  EntryKernel* kernel_;

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

// A mutable meta entry.  Changes get committed to the database when the
// WriteTransaction is destroyed.
class MutableEntry : public Entry {
  friend class WriteTransaction;
  friend class Directory;
  void Init(WriteTransaction* trans, const Id& parent_id,
      const std::string& name);
 public:
  MutableEntry(WriteTransaction* trans, Create, const Id& parent_id,
               const std::string& name);
  MutableEntry(WriteTransaction* trans, CreateNewUpdateItem, const Id& id);
  MutableEntry(WriteTransaction* trans, GetByHandle, int64);
  MutableEntry(WriteTransaction* trans, GetById, const Id&);
  MutableEntry(WriteTransaction* trans, GetByClientTag, const std::string& tag);
  MutableEntry(WriteTransaction* trans, GetByServerTag, const std::string& tag);

  inline WriteTransaction* write_transaction() const {
    return write_transaction_;
  }

  // Field Accessors.  Some of them trigger the re-indexing of the entry.
  // Return true on success, return false on failure, which means
  // that putting the value would have caused a duplicate in the index.
  // TODO(chron): Remove some of these unecessary return values.
  bool Put(Int64Field field, const int64& value);
  bool Put(IdField field, const Id& value);

  // Do a simple property-only update if the PARENT_ID field.  Use with caution.
  //
  // The normal Put(IS_PARENT) call will move the item to the front of the
  // sibling order to maintain the linked list invariants when the parent
  // changes.  That's usually what you want to do, but it's inappropriate
  // when the caller is trying to change the parent ID of a the whole set
  // of children (e.g. because the ID changed during a commit).  For those
  // cases, there's this function.  It will corrupt the sibling ordering
  // if you're not careful.
  void PutParentIdPropertyOnly(const Id& parent_id);

  bool Put(StringField field, const std::string& value);
  bool Put(BaseVersion field, int64 value);

  bool Put(ProtoField field, const sync_pb::EntitySpecifics& value);
  inline bool Put(BitField field, bool value) {
    return PutField(field, value);
  }
  inline bool Put(IsDelField field, bool value) {
    return PutIsDel(value);
  }
  bool Put(IndexedBitField field, bool value);

  // Sets the position of this item, and updates the entry kernels of the
  // adjacent siblings so that list invariants are maintained.  Returns false
  // and fails if |predecessor_id| does not identify a sibling.  Pass the root
  // ID to put the node in first position.
  bool PutPredecessor(const Id& predecessor_id);

  inline bool Put(BitTemp field, bool value) {
    return PutTemp(field, value);
  }

 protected:
  syncable::MetahandleSet* GetDirtyIndexHelper();

  template <typename FieldType, typename ValueType>
  inline bool PutField(FieldType field, const ValueType& value) {
    DCHECK(kernel_);
    if (kernel_->ref(field) != value) {
      kernel_->put(field, value);
      kernel_->mark_dirty(GetDirtyIndexHelper());
    }
    return true;
  }

  template <typename TempType, typename ValueType>
  inline bool PutTemp(TempType field, const ValueType& value) {
    DCHECK(kernel_);
    kernel_->put(field, value);
    return true;
  }

  bool PutIsDel(bool value);

 private:  // Don't allow creation on heap, except by sync API wrappers.
  friend class sync_api::WriteNode;
  void* operator new(size_t size) { return (::operator new)(size); }

  bool PutImpl(StringField field, const std::string& value);
  bool PutUniqueClientTag(const std::string& value);

  // Adjusts the successor and predecessor entries so that they no longer
  // refer to this entry.
  void UnlinkFromOrder();

  // Kind of redundant. We should reduce the number of pointers
  // floating around if at all possible. Could we store this in Directory?
  // Scope: Set on construction, never changed after that.
  WriteTransaction* const write_transaction_;

 protected:
  MutableEntry();

  DISALLOW_COPY_AND_ASSIGN(MutableEntry);
};

class LessParentIdAndHandle;
template <typename FieldType, FieldType field_index>
class LessField;
class LessEntryMetaHandles {
 public:
  inline bool operator()(const syncable::EntryKernel& a,
                         const syncable::EntryKernel& b) const {
    return a.ref(META_HANDLE) < b.ref(META_HANDLE);
  }
};
typedef std::set<EntryKernel, LessEntryMetaHandles> OriginalEntries;

// How syncable indices & Indexers work.
//
// The syncable Directory maintains several indices on the Entries it tracks.
// The indices follow a common pattern:
//   (a) The index allows efficient lookup of an Entry* with particular
//       field values.  This is done by use of a std::set<> and a custom
//       comparator.
//   (b) There may be conditions for inclusion in the index -- for example,
//       deleted items might not be indexed.
//   (c) Because the index set contains only Entry*, one must be careful
//       to remove Entries from the set before updating the value of
//       an indexed field.
// The traits of an index are a Comparator (to define the set ordering) and a
// ShouldInclude function (to define the conditions for inclusion).  For each
// index, the traits are grouped into a class called an Indexer which
// can be used as a template type parameter.

// Traits type for metahandle index.
struct MetahandleIndexer {
  // This index is of the metahandle field values.
  typedef LessField<MetahandleField, META_HANDLE> Comparator;

  // This index includes all entries.
  inline static bool ShouldInclude(const EntryKernel* a) {
    return true;
  }
};

// Traits type for ID field index.
struct IdIndexer {
  // This index is of the ID field values.
  typedef LessField<IdField, ID> Comparator;

  // This index includes all entries.
  inline static bool ShouldInclude(const EntryKernel* a) {
    return true;
  }
};

// Traits type for unique client tag index.
struct ClientTagIndexer {
  // This index is of the client-tag values.
  typedef LessField<StringField, UNIQUE_CLIENT_TAG> Comparator;

  // Items are only in this index if they have a non-empty client tag value.
  static bool ShouldInclude(const EntryKernel* a);
};

// This index contains EntryKernels ordered by parent ID and metahandle.
// It allows efficient lookup of the children of a given parent.
struct ParentIdAndHandleIndexer {
  // This index is of the parent ID and metahandle.  We use a custom
  // comparator.
  class Comparator;

  // This index does not include deleted items.
  static bool ShouldInclude(const EntryKernel* a);
};

// Given an Indexer providing the semantics of an index, defines the
// set type used to actually contain the index.
template <typename Indexer>
struct Index {
  typedef std::set<EntryKernel*, typename Indexer::Comparator> Set;
};

// a WriteTransaction has a writer tag describing which body of code is doing
// the write. This is defined up here since DirectoryChangeEvent also contains
// one.
enum WriterTag {
  INVALID,
  SYNCER,
  AUTHWATCHER,
  UNITTEST,
  VACUUM_AFTER_SAVE,
  PURGE_ENTRIES,
  SYNCAPI
};

// A separate Event type and channel for very frequent changes, caused
// by anything, not just the user.
struct DirectoryChangeEvent {
  enum {
    // Means listener should go through list of original entries and
    // calculate what it needs to notify.  It should *not* call any
    // callbacks or attempt to lock anything because a
    // WriteTransaction is being held until the listener returns.
    CALCULATE_CHANGES,
    // Means the WriteTransaction is ending, and this is the absolute
    // last chance to perform any read operations in the current transaction.
    // It is not recommended that the listener perform any writes.
    TRANSACTION_ENDING,
    // Means the WriteTransaction has been released and the listener
    // can now take action on the changes it calculated.
    TRANSACTION_COMPLETE,
    // Channel is closing.
    SHUTDOWN
  } todo;
  // These members are only valid for CALCULATE_CHANGES.
  const OriginalEntries* originals;
  BaseTransaction* trans;  // This is valid also for TRANSACTION_ENDING
  WriterTag writer;
  typedef DirectoryChangeEvent EventType;
  static inline bool IsChannelShutdownEvent(const EventType& e) {
    return SHUTDOWN == e.todo;
  }
};

// The name Directory in this case means the entire directory
// structure within a single user account.
//
// Sqlite is a little goofy, in that each thread must access a database
// via its own handle.  So, a Directory object should only be accessed
// from a single thread.  Use DirectoryManager's Open() method to
// always get a directory that has been properly initialized on the
// current thread.
//
// The db is protected against concurrent modification by a reader/
// writer lock, negotiated by the ReadTransaction and WriteTransaction
// friend classes.  The in-memory indices are protected against
// concurrent modification by the kernel lock.
//
// All methods which require the reader/writer lock to be held either
//   are protected and only called from friends in a transaction
//   or are public and take a Transaction* argument.
//
// All methods which require the kernel lock to be already held take a
// ScopeKernelLock* argument.
//
// To prevent deadlock, the reader writer transaction lock must always
// be held before acquiring the kernel lock.
class ScopedKernelLock;
class IdFilter;
class DirectoryManager;
struct PathMatcher;

class Directory {
  friend class BaseTransaction;
  friend class Entry;
  friend class MutableEntry;
  friend class ReadTransaction;
  friend class ReadTransactionWithoutDB;
  friend class ScopedKernelLock;
  friend class ScopedKernelUnlock;
  friend class WriteTransaction;
  friend class SyncableDirectoryTest;
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest,
                           TakeSnapshotGetsAllDirtyHandlesTest);
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest,
                           TakeSnapshotGetsOnlyDirtyHandlesTest);
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest, TestPurgeEntriesWithTypeIn);
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest,
                           TakeSnapshotGetsMetahandlesToPurge);

 public:
  class EventListenerHookup;

  // Various data that the Directory::Kernel we are backing (persisting data
  // for) needs saved across runs of the application.
  struct PersistedKernelInfo {
    AutofillMigrationDebugInfo autofill_migration_debug_info;

    PersistedKernelInfo();
    ~PersistedKernelInfo();

    // Set the |download_progress| entry for the given model to a
    // "first sync" start point.  When such a value is sent to the server,
    // a full download of all objects of the model will be initiated.
    void reset_download_progress(ModelType model_type);

    // Last sync timestamp fetched from the server.
    sync_pb::DataTypeProgressMarker download_progress[MODEL_TYPE_COUNT];
    // true iff we ever reached the end of the changelog.
    ModelTypeBitSet initial_sync_ended;
    // The store birthday we were given by the server. Contents are opaque to
    // the client.
    std::string store_birthday;
    // The next local ID that has not been used with this cache-GUID.
    int64 next_id;
    // The persisted notification state.
    std::string notification_state;

    AutofillMigrationState autofill_migration_state;
  };

  // What the Directory needs on initialization to create itself and its Kernel.
  // Filled by DirectoryBackingStore::Load.
  struct KernelLoadInfo {
    PersistedKernelInfo kernel_info;
    std::string cache_guid;  // Created on first initialization, never changes.
    int64 max_metahandle;    // Computed (using sql MAX aggregate) on init.
    KernelLoadInfo() : max_metahandle(0) {
    }
  };

  // The dirty/clean state of kernel fields backed by the share_info table.
  // This is public so it can be used in SaveChangesSnapshot for persistence.
  enum KernelShareInfoStatus {
    KERNEL_SHARE_INFO_INVALID,
    KERNEL_SHARE_INFO_VALID,
    KERNEL_SHARE_INFO_DIRTY
  };

  // When the Directory is told to SaveChanges, a SaveChangesSnapshot is
  // constructed and forms a consistent snapshot of what needs to be sent to
  // the backing store.
  struct SaveChangesSnapshot {
    SaveChangesSnapshot();
    ~SaveChangesSnapshot();

    KernelShareInfoStatus kernel_info_status;
    PersistedKernelInfo kernel_info;
    OriginalEntries dirty_metas;
    MetahandleSet metahandles_to_purge;
  };

  Directory();
  virtual ~Directory();

  DirOpenResult Open(const FilePath& file_path, const std::string& name);

  void Close();

  int64 NextMetahandle();
  // Always returns a negative id.  Positive client ids are generated
  // by the server only.
  Id NextId();

  const FilePath& file_path() const { return kernel_->db_path; }
  bool good() const { return NULL != store_; }

  // The download progress is an opaque token provided by the sync server
  // to indicate the continuation state of the next GetUpdates operation.
  void GetDownloadProgress(
      ModelType type,
      sync_pb::DataTypeProgressMarker* value_out) const;
  void GetDownloadProgressAsString(
      ModelType type,
      std::string* value_out) const;
  void SetDownloadProgress(
      ModelType type,
      const sync_pb::DataTypeProgressMarker& value);

  bool initial_sync_ended_for_type(ModelType type) const;
  void set_initial_sync_ended_for_type(ModelType type, bool value);
  AutofillMigrationState get_autofill_migration_state() const;

  AutofillMigrationDebugInfo get_autofill_migration_debug_info() const;

  void set_autofill_migration_state(AutofillMigrationState state);

  void set_autofill_migration_state_debug_info(
      syncable::AutofillMigrationDebugInfo::PropertyToSet property_to_set,
      const syncable::AutofillMigrationDebugInfo& info);

  const std::string& name() const { return kernel_->name; }

  // (Account) Store birthday is opaque to the client,
  // so we keep it in the format it is in the proto buffer
  // in case we switch to a binary birthday later.
  std::string store_birthday() const;
  void set_store_birthday(std::string store_birthday);

  std::string GetAndClearNotificationState();
  void SetNotificationState(const std::string& notification_state);

  // Unique to each account / client pair.
  std::string cache_guid() const;

  browser_sync::ChannelHookup<DirectoryChangeEvent>* AddChangeObserver(
      browser_sync::ChannelEventHandler<DirectoryChangeEvent>* observer);

 protected:  // for friends, mainly used by Entry constructors
  virtual EntryKernel* GetEntryByHandle(int64 handle);
  virtual EntryKernel* GetEntryByHandle(int64 metahandle,
      ScopedKernelLock* lock);
  virtual EntryKernel* GetEntryById(const Id& id);
  EntryKernel* GetEntryByServerTag(const std::string& tag);
  virtual EntryKernel* GetEntryByClientTag(const std::string& tag);
  EntryKernel* GetRootEntry();
  bool ReindexId(EntryKernel* const entry, const Id& new_id);
  void ReindexParentId(EntryKernel* const entry, const Id& new_parent_id);
  void ClearDirtyMetahandles();

  // These don't do semantic checking.
  // The semantic checking is implemented higher up.
  void UnlinkEntryFromOrder(EntryKernel* entry,
                            WriteTransaction* trans,
                            ScopedKernelLock* lock);

  // Overridden by tests.
  virtual DirectoryBackingStore* CreateBackingStore(
      const std::string& dir_name,
      const FilePath& backing_filepath);

 private:
  // These private versions expect the kernel lock to already be held
  // before calling.
  EntryKernel* GetEntryById(const Id& id, ScopedKernelLock* const lock);

  DirOpenResult OpenImpl(const FilePath& file_path, const std::string& name);

  template <class T> void TestAndSet(T* kernel_data, const T* data_to_set);

  struct DirectoryEventTraits {
    typedef DirectoryEvent EventType;
    static inline bool IsChannelShutdownEvent(const DirectoryEvent& event) {
      return DIRECTORY_DESTROYED == event;
    }
  };
 public:
  typedef EventChannel<DirectoryEventTraits, base::Lock> Channel;
  typedef std::vector<int64> ChildHandles;

  // Returns the child meta handles for given parent id.
  void GetChildHandles(BaseTransaction*, const Id& parent_id,
      const std::string& path_spec, ChildHandles* result);
  void GetChildHandles(BaseTransaction*, const Id& parent_id,
      ChildHandles* result);
  void GetChildHandlesImpl(BaseTransaction* trans, const Id& parent_id,
                           PathMatcher* matcher, ChildHandles* result);

  // Find the first or last child in the positional ordering under a parent,
  // and return its id.  Returns a root Id if parent has no children.
  virtual Id GetFirstChildId(BaseTransaction* trans, const Id& parent_id);
  Id GetLastChildId(BaseTransaction* trans, const Id& parent_id);

  // SaveChanges works by taking a consistent snapshot of the current Directory
  // state and indices (by deep copy) under a ReadTransaction, passing this
  // snapshot to the backing store under no transaction, and finally cleaning
  // up by either purging entries no longer needed (this part done under a
  // WriteTransaction) or rolling back the dirty bits.  It also uses
  // internal locking to enforce SaveChanges operations are mutually exclusive.
  //
  // WARNING: THIS METHOD PERFORMS SYNCHRONOUS I/O VIA SQLITE.
  bool SaveChanges();

  // Returns the number of entities with the unsynced bit set.
  int64 unsynced_entity_count() const;

  // Get GetUnsyncedMetaHandles should only be called after SaveChanges and
  // before any new entries have been created. The intention is that the
  // syncer should call it from its PerformSyncQueries member.
  typedef std::vector<int64> UnsyncedMetaHandles;
  void GetUnsyncedMetaHandles(BaseTransaction* trans,
                              UnsyncedMetaHandles* result);

  // Get all the metahandles for unapplied updates
  typedef std::vector<int64> UnappliedUpdateMetaHandles;
  void GetUnappliedUpdateMetaHandles(BaseTransaction* trans,
                                     UnappliedUpdateMetaHandles* result);

  // Get the channel for post save notification, used by the syncer.
  inline Channel* channel() const {
    return kernel_->channel;
  }

  // Checks tree metadata consistency.
  // If full_scan is false, the function will avoid pulling any entries from the
  // db and scan entries currently in ram.
  // If full_scan is true, all entries will be pulled from the database.
  // No return value, CHECKs will be triggered if we're given bad
  // information.
  void CheckTreeInvariants(syncable::BaseTransaction* trans,
                           bool full_scan);

  void CheckTreeInvariants(syncable::BaseTransaction* trans,
                           const OriginalEntries* originals);

  void CheckTreeInvariants(syncable::BaseTransaction* trans,
                           const MetahandleSet& handles,
                           const IdFilter& idfilter);

  // Purges all data associated with any entries whose ModelType or
  // ServerModelType is found in |types|, from _both_ memory and disk.
  // Only  valid, "real" model types are allowed in |types| (see model_type.h
  // for definitions).  "Purge" is just meant to distinguish from "deleting"
  // entries, which means something different in the syncable namespace.
  // WARNING! This can be real slow, as it iterates over all entries.
  // WARNING! Performs synchronous I/O.
  virtual void PurgeEntriesWithTypeIn(const std::set<ModelType>& types);

 private:
  // Helper to prime ids_index, parent_id_and_names_index, unsynced_metahandles
  // and unapplied_metahandles from metahandles_index.
  void InitializeIndices();

  // Constructs a consistent snapshot of the current Directory state and
  // indices (by deep copy) under a ReadTransaction for use in |snapshot|.
  // See SaveChanges() for more information.
  void TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot);

  // Purges from memory any unused, safe to remove entries that were
  // successfully deleted on disk as a result of the SaveChanges that processed
  // |snapshot|.  See SaveChanges() for more information.
  void VacuumAfterSaveChanges(const SaveChangesSnapshot& snapshot);

  // Rolls back dirty bits in the event that the SaveChanges that
  // processed |snapshot| failed, for example, due to no disk space.
  void HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot);

  // For new entry creation only
  void InsertEntry(EntryKernel* entry, ScopedKernelLock* lock);
  void InsertEntry(EntryKernel* entry);

  // Used by CheckTreeInvariants
  void GetAllMetaHandles(BaseTransaction* trans, MetahandleSet* result);
  bool SafeToPurgeFromMemory(const EntryKernel* const entry) const;

  // Helper method used to implement GetFirstChildId/GetLastChildId.
  Id GetChildWithNullIdField(IdField field,
                             BaseTransaction* trans,
                             const Id& parent_id);

  // Internal setters that do not acquire a lock internally.  These are unsafe
  // on their own; caller must guarantee exclusive access manually by holding
  // a ScopedKernelLock.
  void set_initial_sync_ended_for_type_unsafe(ModelType type, bool x);
  void SetNotificationStateUnsafe(const std::string& notification_state);

  Directory& operator = (const Directory&);

 public:
  typedef Index<MetahandleIndexer>::Set MetahandlesIndex;
  typedef Index<IdIndexer>::Set IdsIndex;
  // All entries in memory must be in both the MetahandlesIndex and
  // the IdsIndex, but only non-deleted entries will be the
  // ParentIdChildIndex.
  typedef Index<ParentIdAndHandleIndexer>::Set ParentIdChildIndex;

  // Contains both deleted and existing entries with tags.
  // We can't store only existing tags because the client would create
  // items that had a duplicated ID in the end, resulting in a DB key
  // violation. ID reassociation would fail after an attempted commit.
  typedef Index<ClientTagIndexer>::Set ClientTagIndex;

 protected:
  // Used by tests.
  void init_kernel(const std::string& name);

 private:

  struct Kernel {
    Kernel(const FilePath& db_path, const std::string& name,
           const KernelLoadInfo& info);

    ~Kernel();

    void AddRef();  // For convenience.
    void Release();

    FilePath const db_path;
    // TODO(timsteele): audit use of the member and remove if possible
    volatile base::subtle::AtomicWord refcount;

    // Implements ReadTransaction / WriteTransaction using a simple lock.
    base::Lock transaction_mutex;

    // The name of this directory.
    std::string const name;

    // Protects all members below.
    // The mutex effectively protects all the indices, but not the
    // entries themselves.  So once a pointer to an entry is pulled
    // from the index, the mutex can be unlocked and entry read or written.
    //
    // Never hold the mutex and do anything with the database or any
    // other buffered IO.  Violating this rule will result in deadlock.
    base::Lock mutex;
    // Entries indexed by metahandle
    MetahandlesIndex* metahandles_index;
    // Entries indexed by id
    IdsIndex* ids_index;
    ParentIdChildIndex* parent_id_child_index;
    ClientTagIndex* client_tag_index;
    // So we don't have to create an EntryKernel every time we want to
    // look something up in an index.  Needle in haystack metaphor.
    EntryKernel needle;

    // 3 in-memory indices on bits used extremely frequently by the syncer.
    MetahandleSet* const unapplied_update_metahandles;
    MetahandleSet* const unsynced_metahandles;
    // Contains metahandles that are most likely dirty (though not
    // necessarily).  Dirtyness is confirmed in TakeSnapshotForSaveChanges().
    MetahandleSet* const dirty_metahandles;

    // When a purge takes place, we remove items from all our indices and stash
    // them in here so that SaveChanges can persist their permanent deletion.
    MetahandleSet* const metahandles_to_purge;

    // TODO(ncarter): Figure out what the hell this is, and comment it.
    Channel* const channel;

    // The changes channel mutex is explicit because it must be locked
    // while holding the transaction mutex and released after
    // releasing the transaction mutex.
    browser_sync::Channel<DirectoryChangeEvent> changes_channel;

    base::Lock changes_channel_mutex;
    KernelShareInfoStatus info_status;

    // These 3 members are backed in the share_info table, and
    // their state is marked by the flag above.

    // A structure containing the Directory state that is written back into the
    // database on SaveChanges.
    PersistedKernelInfo persisted_info;

    // A unique identifier for this account's cache db, used to generate
    // unique server IDs. No need to lock, only written at init time.
    std::string cache_guid;

    // It doesn't make sense for two threads to run SaveChanges at the same
    // time; this mutex protects that activity.
    base::Lock save_changes_mutex;

    // The next metahandle is protected by kernel mutex.
    int64 next_metahandle;

    // Keep a history of recently flushed metahandles for debugging
    // purposes.  Protected by the save_changes_mutex.
    DebugQueue<int64, 1000> flushed_metahandles;
  };

  Kernel* kernel_;

  DirectoryBackingStore* store_;
};

class ScopedKernelLock {
 public:
  explicit ScopedKernelLock(const Directory*);
  ~ScopedKernelLock() {}

  base::AutoLock scoped_lock_;
  Directory* const dir_;
  DISALLOW_COPY_AND_ASSIGN(ScopedKernelLock);
};

// Transactions are now processed FIFO with a straight lock
class BaseTransaction {
  friend class Entry;
 public:
  inline Directory* directory() const { return directory_; }
  inline Id root_id() const { return Id(); }

  virtual ~BaseTransaction();

 protected:
  BaseTransaction(Directory* directory, const char* name,
                  const char* source_file, int line, WriterTag writer);

  // For unit testing. Everything will be mocked out no point initializing.
  explicit BaseTransaction(Directory* directory);

  void UnlockAndLog(OriginalEntries* entries);
  bool NotifyTransactionChangingAndEnding(OriginalEntries* entries);
  virtual void NotifyTransactionComplete();

  Directory* const directory_;
  Directory::Kernel* const dirkernel_;  // for brevity
  const char* const name_;
  base::TimeTicks time_acquired_;
  const char* const source_file_;
  const int line_;
  WriterTag writer_;

 private:
  void Lock();

  DISALLOW_COPY_AND_ASSIGN(BaseTransaction);
};

// Locks db in constructor, unlocks in destructor.
class ReadTransaction : public BaseTransaction {
 public:
  ReadTransaction(Directory* directory, const char* source_file,
                  int line);
  ReadTransaction(const ScopedDirLookup& scoped_dir,
                  const char* source_file, int line);

  virtual ~ReadTransaction();

 protected:  // Don't allow creation on heap, except by sync API wrapper.
  friend class sync_api::ReadTransaction;
  void* operator new(size_t size) { return (::operator new)(size); }

  DISALLOW_COPY_AND_ASSIGN(ReadTransaction);
};

// Locks db in constructor, unlocks in destructor.
class WriteTransaction : public BaseTransaction {
  friend class MutableEntry;
 public:
  explicit WriteTransaction(Directory* directory, WriterTag writer,
                            const char* source_file, int line);
  explicit WriteTransaction(const ScopedDirLookup& directory,
                            WriterTag writer, const char* source_file,
                            int line);
  virtual ~WriteTransaction();

  void SaveOriginal(EntryKernel* entry);

 protected:
  // Before an entry gets modified, we copy the original into a list
  // so that we can issue change notifications when the transaction
  // is done.
  OriginalEntries* const originals_;

  explicit WriteTransaction(Directory *directory);

  DISALLOW_COPY_AND_ASSIGN(WriteTransaction);
};

bool IsLegalNewParent(BaseTransaction* trans, const Id& id, const Id& parentid);

int64 Now();

// This function sets only the flags needed to get this entry to sync.
void MarkForSyncing(syncable::MutableEntry* e);

// This is not a reset.  It just sets the numeric fields which are not
// initialized by the constructor to zero.
void ZeroFields(EntryKernel* entry, int first_field);

}  // namespace syncable

std::ostream& operator <<(std::ostream&, const syncable::Blob&);

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_H_
