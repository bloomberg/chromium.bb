// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/perftimer.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/protocol/proto_value_conversions.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_change_delegate.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"
#include "chrome/browser/sync/syncable/syncable_columns.h"
#include "chrome/browser/sync/syncable/syncable_enum_conversions.h"
#include "chrome/browser/sync/syncable/transaction_observer.h"
#include "chrome/browser/sync/util/logging.h"
#include "net/base/escape.h"

namespace {
enum InvariantCheckLevel {
  OFF = 0,
  VERIFY_IN_MEMORY = 1,
  FULL_DB_VERIFICATION = 2
};

static const InvariantCheckLevel kInvariantCheckLevel = VERIFY_IN_MEMORY;

// Max number of milliseconds to spend checking syncable entry invariants
static const int kInvariantCheckMaxMs = 50;
}  // namespace

using std::string;

namespace syncable {

#define ENUM_CASE(x) case x: return #x; break

std::string WriterTagToString(WriterTag writer_tag) {
  switch (writer_tag) {
    ENUM_CASE(INVALID);
    ENUM_CASE(SYNCER);
    ENUM_CASE(AUTHWATCHER);
    ENUM_CASE(UNITTEST);
    ENUM_CASE(VACUUM_AFTER_SAVE);
    ENUM_CASE(PURGE_ENTRIES);
    ENUM_CASE(SYNCAPI);
  };
  NOTREACHED();
  return "";
}

#undef ENUM_CASE

WriteTransactionInfo::WriteTransactionInfo(
    int64 id,
    tracked_objects::Location location,
    WriterTag writer,
    ImmutableEntryKernelMutationMap mutations)
    : id(id),
      location_string(location.ToString()),
      writer(writer),
      mutations(mutations) {}

WriteTransactionInfo::WriteTransactionInfo()
    : id(-1), writer(INVALID) {}

WriteTransactionInfo::~WriteTransactionInfo() {}

base::DictionaryValue* WriteTransactionInfo::ToValue(
    size_t max_mutations_size) const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("id", base::Int64ToString(id));
  dict->SetString("location", location_string);
  dict->SetString("writer", WriterTagToString(writer));
  Value* mutations_value = NULL;
  const size_t mutations_size = mutations.Get().size();
  if (mutations_size <= max_mutations_size) {
    mutations_value = EntryKernelMutationMapToValue(mutations.Get());
  } else {
    mutations_value =
        Value::CreateStringValue(
            base::Uint64ToString(static_cast<uint64>(mutations_size)) +
            " mutations");
  }
  dict->Set("mutations", mutations_value);
  return dict;
}

DictionaryValue* EntryKernelMutationToValue(
    const EntryKernelMutation& mutation) {
  DictionaryValue* dict = new DictionaryValue();
  dict->Set("original", mutation.original.ToValue());
  dict->Set("mutated", mutation.mutated.ToValue());
  return dict;
}

ListValue* EntryKernelMutationMapToValue(
    const EntryKernelMutationMap& mutations) {
  ListValue* list = new ListValue();
  for (EntryKernelMutationMap::const_iterator it = mutations.begin();
       it != mutations.end(); ++it) {
    list->Append(EntryKernelMutationToValue(it->second));
  }
  return list;
}

namespace {

// A ScopedIndexUpdater temporarily removes an entry from an index,
// and restores it to the index when the scope exits.  This simplifies
// the common pattern where items need to be removed from an index
// before updating the field.
//
// This class is parameterized on the Indexer traits type, which
// must define a Comparator and a static bool ShouldInclude
// function for testing whether the item ought to be included
// in the index.
template<typename Indexer>
class ScopedIndexUpdater {
 public:
  ScopedIndexUpdater(const ScopedKernelLock& proof_of_lock,
                     EntryKernel* entry,
                     typename Index<Indexer>::Set* index)
      : entry_(entry),
        index_(index) {
    // First call to ShouldInclude happens before the field is updated.
    if (Indexer::ShouldInclude(entry_)) {
      CHECK(index_->erase(entry_));
    }
  }

  ~ScopedIndexUpdater() {
    // Second call to ShouldInclude happens after the field is updated.
    if (Indexer::ShouldInclude(entry_)) {
      CHECK(index_->insert(entry_).second);
    }
  }
 private:
  // The entry that was temporarily removed from the index.
  EntryKernel* entry_;
  // The index which we are updating.
  typename Index<Indexer>::Set* const index_;
};

// Helper function to add an item to the index, if it ought to be added.
template<typename Indexer>
void InitializeIndexEntry(EntryKernel* entry,
                          typename Index<Indexer>::Set* index) {
  if (Indexer::ShouldInclude(entry)) {
    index->insert(entry);
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////
// Comparator and filter functions for the indices.

// static
bool ClientTagIndexer::ShouldInclude(const EntryKernel* a) {
  return !a->ref(UNIQUE_CLIENT_TAG).empty();
}

bool ParentIdAndHandleIndexer::Comparator::operator() (
    const syncable::EntryKernel* a,
    const syncable::EntryKernel* b) const {
  int cmp = a->ref(PARENT_ID).compare(b->ref(PARENT_ID));
  if (cmp != 0)
    return cmp < 0;

  int64 a_position = a->ref(SERVER_POSITION_IN_PARENT);
  int64 b_position = b->ref(SERVER_POSITION_IN_PARENT);
  if (a_position != b_position)
    return a_position < b_position;

  cmp = a->ref(ID).compare(b->ref(ID));
  return cmp < 0;
}

// static
bool ParentIdAndHandleIndexer::ShouldInclude(const EntryKernel* a) {
  // This index excludes deleted items and the root item.  The root
  // item is excluded so that it doesn't show up as a child of itself.
  return !a->ref(IS_DEL) && !a->ref(ID).IsRoot();
}

///////////////////////////////////////////////////////////////////////////
// EntryKernel

EntryKernel::EntryKernel() : dirty_(false) {
  // Everything else should already be default-initialized.
  for (int i = INT64_FIELDS_BEGIN; i < INT64_FIELDS_END; ++i) {
    int64_fields[i] = 0;
  }
}

EntryKernel::~EntryKernel() {}

bool EntryKernel::ContainsString(const std::string& lowercase_query) const {
  // TODO(lipalani) - figure out what to do if the node is encrypted.
  const sync_pb::EntitySpecifics& specifics = ref(SPECIFICS);
  std::string temp;
  // The protobuf serialized string contains the original strings. So
  // we will just serialize it and search it.
  specifics.SerializeToString(&temp);

  // Now convert to lower case.
  StringToLowerASCII(&temp);

  if (temp.find(lowercase_query) != std::string::npos)
    return true;

  // Now go through all the string fields to see if the value is there.
  for (int i = STRING_FIELDS_BEGIN; i < STRING_FIELDS_END; ++i) {
    if (StringToLowerASCII(ref(static_cast<StringField>(i))).find(
            lowercase_query) != std::string::npos)
      return true;
  }

  for (int i = ID_FIELDS_BEGIN; i < ID_FIELDS_END; ++i) {
    const Id& id = ref(static_cast<IdField>(i));
    if (id.ContainsStringCaseInsensitive(lowercase_query)) {
      return true;
    }
  }
  return false;
}

namespace {

// Utility function to loop through a set of enum values and add the
// field keys/values in the kernel to the given dictionary.
//
// V should be convertible to Value.
template <class T, class U, class V>
void SetFieldValues(const EntryKernel& kernel,
                    DictionaryValue* dictionary_value,
                    const char* (*enum_key_fn)(T),
                    V* (*enum_value_fn)(U),
                    int field_key_min, int field_key_max) {
  DCHECK_LE(field_key_min, field_key_max);
  for (int i = field_key_min; i <= field_key_max; ++i) {
    T field = static_cast<T>(i);
    const std::string& key = enum_key_fn(field);
    V* value = enum_value_fn(kernel.ref(field));
    dictionary_value->Set(key, value);
  }
}

// Helper functions for SetFieldValues().

StringValue* Int64ToValue(int64 i) {
  return Value::CreateStringValue(base::Int64ToString(i));
}

StringValue* TimeToValue(const base::Time& t) {
  return Value::CreateStringValue(browser_sync::GetTimeDebugString(t));
}

StringValue* IdToValue(const Id& id) {
  return id.ToValue();
}

}  // namespace

DictionaryValue* EntryKernel::ToValue() const {
  DictionaryValue* kernel_info = new DictionaryValue();
  kernel_info->SetBoolean("isDirty", is_dirty());

  // Int64 fields.
  SetFieldValues(*this, kernel_info,
                 &GetMetahandleFieldString, &Int64ToValue,
                 INT64_FIELDS_BEGIN, META_HANDLE);
  SetFieldValues(*this, kernel_info,
                 &GetBaseVersionString, &Int64ToValue,
                 META_HANDLE + 1, BASE_VERSION);
  SetFieldValues(*this, kernel_info,
                 &GetInt64FieldString, &Int64ToValue,
                 BASE_VERSION + 1, INT64_FIELDS_END - 1);

  // Time fields.
  SetFieldValues(*this, kernel_info,
                 &GetTimeFieldString, &TimeToValue,
                 TIME_FIELDS_BEGIN, TIME_FIELDS_END - 1);

  // ID fields.
  SetFieldValues(*this, kernel_info,
                 &GetIdFieldString, &IdToValue,
                 ID_FIELDS_BEGIN, ID_FIELDS_END - 1);

  // Bit fields.
  SetFieldValues(*this, kernel_info,
                 &GetIndexedBitFieldString, &Value::CreateBooleanValue,
                 BIT_FIELDS_BEGIN, INDEXED_BIT_FIELDS_END - 1);
  SetFieldValues(*this, kernel_info,
                 &GetIsDelFieldString, &Value::CreateBooleanValue,
                 INDEXED_BIT_FIELDS_END, IS_DEL);
  SetFieldValues(*this, kernel_info,
                 &GetBitFieldString, &Value::CreateBooleanValue,
                 IS_DEL + 1, BIT_FIELDS_END - 1);

  // String fields.
  {
    // Pick out the function overload we want.
    StringValue* (*string_to_value)(const std::string&) =
        &Value::CreateStringValue;
    SetFieldValues(*this, kernel_info,
                   &GetStringFieldString, string_to_value,
                   STRING_FIELDS_BEGIN, STRING_FIELDS_END - 1);
  }

  // Proto fields.
  SetFieldValues(*this, kernel_info,
                 &GetProtoFieldString, &browser_sync::EntitySpecificsToValue,
                 PROTO_FIELDS_BEGIN, PROTO_FIELDS_END - 1);

  // Bit temps.
  SetFieldValues(*this, kernel_info,
                 &GetBitTempString, &Value::CreateBooleanValue,
                 BIT_TEMPS_BEGIN, BIT_TEMPS_END - 1);

  return kernel_info;
}

///////////////////////////////////////////////////////////////////////////
// Directory

void Directory::InitKernelForTest(
    const std::string& name,
    DirectoryChangeDelegate* delegate,
    const browser_sync::WeakHandle<TransactionObserver>&
        transaction_observer) {
  DCHECK(!kernel_);
  kernel_ =  new Kernel(FilePath(), name, KernelLoadInfo(),
                        delegate, transaction_observer);
}

Directory::PersistedKernelInfo::PersistedKernelInfo()
    : next_id(0) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    reset_download_progress(ModelTypeFromInt(i));
  }
}

Directory::PersistedKernelInfo::~PersistedKernelInfo() {}

void Directory::PersistedKernelInfo::reset_download_progress(
    ModelType model_type) {
  download_progress[model_type].set_data_type_id(
      GetExtensionFieldNumberFromModelType(model_type));
  // An empty-string token indicates no prior knowledge.
  download_progress[model_type].set_token(std::string());
}

Directory::SaveChangesSnapshot::SaveChangesSnapshot()
    : kernel_info_status(KERNEL_SHARE_INFO_INVALID) {
}

Directory::SaveChangesSnapshot::~SaveChangesSnapshot() {}

Directory::Kernel::Kernel(
    const FilePath& db_path, const string& name,
    const KernelLoadInfo& info, DirectoryChangeDelegate* delegate,
    const browser_sync::WeakHandle<TransactionObserver>&
        transaction_observer)
    : db_path(db_path),
      refcount(1),
      next_write_transaction_id(0),
      name(name),
      metahandles_index(new Directory::MetahandlesIndex),
      ids_index(new Directory::IdsIndex),
      parent_id_child_index(new Directory::ParentIdChildIndex),
      client_tag_index(new Directory::ClientTagIndex),
      unapplied_update_metahandles(new MetahandleSet),
      unsynced_metahandles(new MetahandleSet),
      dirty_metahandles(new MetahandleSet),
      metahandles_to_purge(new MetahandleSet),
      info_status(Directory::KERNEL_SHARE_INFO_VALID),
      persisted_info(info.kernel_info),
      cache_guid(info.cache_guid),
      next_metahandle(info.max_metahandle + 1),
      delegate(delegate),
      transaction_observer(transaction_observer) {
  DCHECK(delegate);
  DCHECK(transaction_observer.IsInitialized());
}

void Directory::Kernel::AddRef() {
  base::subtle::NoBarrier_AtomicIncrement(&refcount, 1);
}

void Directory::Kernel::Release() {
  if (!base::subtle::NoBarrier_AtomicIncrement(&refcount, -1))
    delete this;
}

Directory::Kernel::~Kernel() {
  CHECK_EQ(0, refcount);
  delete unsynced_metahandles;
  delete unapplied_update_metahandles;
  delete dirty_metahandles;
  delete metahandles_to_purge;
  delete parent_id_child_index;
  delete client_tag_index;
  delete ids_index;
  STLDeleteElements(metahandles_index);
  delete metahandles_index;
}

Directory::Directory() : kernel_(NULL), store_(NULL) {
}

Directory::~Directory() {
  Close();
}

DirOpenResult Directory::Open(
    const FilePath& file_path, const string& name,
    DirectoryChangeDelegate* delegate,
    const browser_sync::WeakHandle<TransactionObserver>&
        transaction_observer) {
  const DirOpenResult result =
      OpenImpl(file_path, name, delegate, transaction_observer);
  if (OPENED != result)
    Close();
  return result;
}

void Directory::InitializeIndices() {
  MetahandlesIndex::iterator it = kernel_->metahandles_index->begin();
  for (; it != kernel_->metahandles_index->end(); ++it) {
    EntryKernel* entry = *it;
    InitializeIndexEntry<ParentIdAndHandleIndexer>(entry,
        kernel_->parent_id_child_index);
    InitializeIndexEntry<IdIndexer>(entry, kernel_->ids_index);
    InitializeIndexEntry<ClientTagIndexer>(entry, kernel_->client_tag_index);
    if (entry->ref(IS_UNSYNCED))
      kernel_->unsynced_metahandles->insert(entry->ref(META_HANDLE));
    if (entry->ref(IS_UNAPPLIED_UPDATE))
      kernel_->unapplied_update_metahandles->insert(entry->ref(META_HANDLE));
    DCHECK(!entry->is_dirty());
  }
}

DirectoryBackingStore* Directory::CreateBackingStore(
    const string& dir_name, const FilePath& backing_filepath) {
  return new DirectoryBackingStore(dir_name, backing_filepath);
}

DirOpenResult Directory::OpenImpl(
    const FilePath& file_path,
    const string& name,
    DirectoryChangeDelegate* delegate,
    const browser_sync::WeakHandle<TransactionObserver>&
        transaction_observer) {
  DCHECK_EQ(static_cast<DirectoryBackingStore*>(NULL), store_);
  FilePath db_path(file_path);
  file_util::AbsolutePath(&db_path);
  store_ = CreateBackingStore(name, db_path);

  KernelLoadInfo info;
  // Temporary indices before kernel_ initialized in case Load fails. We 0(1)
  // swap these later.
  MetahandlesIndex metas_bucket;
  DirOpenResult result = store_->Load(&metas_bucket, &info);
  if (OPENED != result)
    return result;

  kernel_ = new Kernel(db_path, name, info, delegate, transaction_observer);
  kernel_->metahandles_index->swap(metas_bucket);
  InitializeIndices();
  return OPENED;
}

void Directory::Close() {
  if (store_)
    delete store_;
  store_ = NULL;
  if (kernel_) {
    bool del = !base::subtle::NoBarrier_AtomicIncrement(&kernel_->refcount, -1);
    DCHECK(del) << "Kernel should only have a single ref";
    if (del)
      delete kernel_;
    kernel_ = NULL;
  }
}

EntryKernel* Directory::GetEntryById(const Id& id) {
  ScopedKernelLock lock(this);
  return GetEntryById(id, &lock);
}

EntryKernel* Directory::GetEntryById(const Id& id,
                                     ScopedKernelLock* const lock) {
  DCHECK(kernel_);
  // Find it in the in memory ID index.
  kernel_->needle.put(ID, id);
  IdsIndex::iterator id_found = kernel_->ids_index->find(&kernel_->needle);
  if (id_found != kernel_->ids_index->end()) {
    return *id_found;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByClientTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  // Find it in the ClientTagIndex.
  kernel_->needle.put(UNIQUE_CLIENT_TAG, tag);
  ClientTagIndex::iterator found = kernel_->client_tag_index->find(
      &kernel_->needle);
  if (found != kernel_->client_tag_index->end()) {
    return *found;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByServerTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  // We don't currently keep a separate index for the tags.  Since tags
  // only exist for server created items that are the first items
  // to be created in a store, they should have small metahandles.
  // So, we just iterate over the items in sorted metahandle order,
  // looking for a match.
  MetahandlesIndex& set = *kernel_->metahandles_index;
  for (MetahandlesIndex::iterator i = set.begin(); i != set.end(); ++i) {
    if ((*i)->ref(UNIQUE_SERVER_TAG) == tag) {
      return *i;
    }
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByHandle(int64 metahandle) {
  ScopedKernelLock lock(this);
  return GetEntryByHandle(metahandle, &lock);
}

EntryKernel* Directory::GetEntryByHandle(int64 metahandle,
                                         ScopedKernelLock* lock) {
  // Look up in memory
  kernel_->needle.put(META_HANDLE, metahandle);
  MetahandlesIndex::iterator found =
      kernel_->metahandles_index->find(&kernel_->needle);
  if (found != kernel_->metahandles_index->end()) {
    // Found it in memory.  Easy.
    return *found;
  }
  return NULL;
}

void Directory::GetChildHandlesById(
    BaseTransaction* trans, const Id& parent_id,
    Directory::ChildHandles* result) {
  CHECK(this == trans->directory());
  result->clear();

  ScopedKernelLock lock(this);
  AppendChildHandles(lock, parent_id, result);
}

void Directory::GetChildHandlesByHandle(
    BaseTransaction* trans, int64 handle,
    Directory::ChildHandles* result) {
  CHECK(this == trans->directory());
  result->clear();

  ScopedKernelLock lock(this);
  EntryKernel* kernel = GetEntryByHandle(handle, &lock);
  if (!kernel)
    return;

  AppendChildHandles(lock, kernel->ref(ID), result);
}

EntryKernel* Directory::GetRootEntry() {
  return GetEntryById(Id());
}

void Directory::InsertEntry(EntryKernel* entry) {
  ScopedKernelLock lock(this);
  InsertEntry(entry, &lock);
}

void Directory::InsertEntry(EntryKernel* entry, ScopedKernelLock* lock) {
  DCHECK(NULL != lock);
  CHECK(NULL != entry);
  static const char error[] = "Entry already in memory index.";
  CHECK(kernel_->metahandles_index->insert(entry).second) << error;

  if (!entry->ref(IS_DEL)) {
    CHECK(kernel_->parent_id_child_index->insert(entry).second) << error;
  }
  CHECK(kernel_->ids_index->insert(entry).second) << error;

  // Should NEVER be created with a client tag.
  CHECK(entry->ref(UNIQUE_CLIENT_TAG).empty());
}

bool Directory::ReindexId(EntryKernel* const entry, const Id& new_id) {
  ScopedKernelLock lock(this);
  if (NULL != GetEntryById(new_id, &lock))
    return false;

  {
    // Update the indices that depend on the ID field.
    ScopedIndexUpdater<IdIndexer> updater_a(lock, entry, kernel_->ids_index);
    ScopedIndexUpdater<ParentIdAndHandleIndexer> updater_b(lock, entry,
        kernel_->parent_id_child_index);
    entry->put(ID, new_id);
  }
  return true;
}

void Directory::ReindexParentId(EntryKernel* const entry,
                                const Id& new_parent_id) {
  ScopedKernelLock lock(this);

  {
    // Update the indices that depend on the PARENT_ID field.
    ScopedIndexUpdater<ParentIdAndHandleIndexer> index_updater(lock, entry,
        kernel_->parent_id_child_index);
    entry->put(PARENT_ID, new_parent_id);
  }
}

void Directory::ClearDirtyMetahandles() {
  kernel_->transaction_mutex.AssertAcquired();
  kernel_->dirty_metahandles->clear();
}

bool Directory::SafeToPurgeFromMemory(const EntryKernel* const entry) const {
  bool safe = entry->ref(IS_DEL) && !entry->is_dirty() &&
      !entry->ref(SYNCING) && !entry->ref(IS_UNAPPLIED_UPDATE) &&
      !entry->ref(IS_UNSYNCED);

  if (safe) {
    int64 handle = entry->ref(META_HANDLE);
    CHECK_EQ(kernel_->dirty_metahandles->count(handle), 0U);
    // TODO(tim): Bug 49278.
    CHECK(!kernel_->unsynced_metahandles->count(handle));
    CHECK(!kernel_->unapplied_update_metahandles->count(handle));
  }

  return safe;
}

void Directory::TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot) {
  ReadTransaction trans(FROM_HERE, this);
  ScopedKernelLock lock(this);
  // Deep copy dirty entries from kernel_->metahandles_index into snapshot and
  // clear dirty flags.

  for (MetahandleSet::const_iterator i = kernel_->dirty_metahandles->begin();
       i != kernel_->dirty_metahandles->end(); ++i) {
    EntryKernel* entry = GetEntryByHandle(*i, &lock);
    if (!entry)
      continue;
    // Skip over false positives; it happens relatively infrequently.
    if (!entry->is_dirty())
      continue;
    snapshot->dirty_metas.insert(snapshot->dirty_metas.end(), *entry);
    DCHECK_EQ(1U, kernel_->dirty_metahandles->count(*i));
    // We don't bother removing from the index here as we blow the entire thing
    // in a moment, and it unnecessarily complicates iteration.
    entry->clear_dirty(NULL);
  }
  ClearDirtyMetahandles();

  // Set purged handles.
  DCHECK(snapshot->metahandles_to_purge.empty());
  snapshot->metahandles_to_purge.swap(*(kernel_->metahandles_to_purge));

  // Fill kernel_info_status and kernel_info.
  snapshot->kernel_info = kernel_->persisted_info;
  // To avoid duplicates when the process crashes, we record the next_id to be
  // greater magnitude than could possibly be reached before the next save
  // changes.  In other words, it's effectively impossible for the user to
  // generate 65536 new bookmarks in 3 seconds.
  snapshot->kernel_info.next_id -= 65536;
  snapshot->kernel_info_status = kernel_->info_status;
  // This one we reset on failure.
  kernel_->info_status = KERNEL_SHARE_INFO_VALID;
}

bool Directory::SaveChanges() {
  bool success = false;
  DCHECK(store_);

  base::AutoLock scoped_lock(kernel_->save_changes_mutex);

  // Snapshot and save.
  SaveChangesSnapshot snapshot;
  TakeSnapshotForSaveChanges(&snapshot);
  success = store_->SaveChanges(snapshot);

  // Handle success or failure.
  if (success)
    VacuumAfterSaveChanges(snapshot);
  else
    HandleSaveChangesFailure(snapshot);
  return success;
}

void Directory::VacuumAfterSaveChanges(const SaveChangesSnapshot& snapshot) {
  if (snapshot.dirty_metas.empty())
    return;

  // Need a write transaction as we are about to permanently purge entries.
  WriteTransaction trans(FROM_HERE, VACUUM_AFTER_SAVE, this);
  ScopedKernelLock lock(this);
  // Now drop everything we can out of memory.
  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    kernel_->needle.put(META_HANDLE, i->ref(META_HANDLE));
    MetahandlesIndex::iterator found =
        kernel_->metahandles_index->find(&kernel_->needle);
    EntryKernel* entry = (found == kernel_->metahandles_index->end() ?
                          NULL : *found);
    if (entry && SafeToPurgeFromMemory(entry)) {
      // We now drop deleted metahandles that are up to date on both the client
      // and the server.
      size_t num_erased = 0;
      num_erased = kernel_->ids_index->erase(entry);
      DCHECK_EQ(1u, num_erased);
      num_erased = kernel_->metahandles_index->erase(entry);
      DCHECK_EQ(1u, num_erased);

      // Might not be in it
      num_erased = kernel_->client_tag_index->erase(entry);
      DCHECK_EQ(entry->ref(UNIQUE_CLIENT_TAG).empty(), !num_erased);
      CHECK(!kernel_->parent_id_child_index->count(entry));
      delete entry;
    }
  }
}

void Directory::PurgeEntriesWithTypeIn(const std::set<ModelType>& types) {
  if (types.count(UNSPECIFIED) != 0U || types.count(TOP_LEVEL_FOLDER) != 0U) {
    NOTREACHED() << "Don't support purging unspecified or top level entries.";
    return;
  }

  if (types.empty())
    return;

  {
    WriteTransaction trans(FROM_HERE, PURGE_ENTRIES, this);
    {
      ScopedKernelLock lock(this);
      MetahandlesIndex::iterator it = kernel_->metahandles_index->begin();
      while (it != kernel_->metahandles_index->end()) {
        const sync_pb::EntitySpecifics& local_specifics = (*it)->ref(SPECIFICS);
        const sync_pb::EntitySpecifics& server_specifics =
            (*it)->ref(SERVER_SPECIFICS);
        ModelType local_type = GetModelTypeFromSpecifics(local_specifics);
        ModelType server_type = GetModelTypeFromSpecifics(server_specifics);

        // Note the dance around incrementing |it|, since we sometimes erase().
        if (types.count(local_type) > 0 || types.count(server_type) > 0) {
          UnlinkEntryFromOrder(*it, NULL, &lock);
          int64 handle = (*it)->ref(META_HANDLE);
          kernel_->metahandles_to_purge->insert(handle);

          size_t num_erased = 0;
          EntryKernel* entry = *it;
          num_erased = kernel_->ids_index->erase(entry);
          DCHECK_EQ(1u, num_erased);
          num_erased = kernel_->client_tag_index->erase(entry);
          DCHECK_EQ(entry->ref(UNIQUE_CLIENT_TAG).empty(), !num_erased);
          num_erased = kernel_->unsynced_metahandles->erase(handle);
          DCHECK_EQ(entry->ref(IS_UNSYNCED), num_erased > 0);
          num_erased = kernel_->unapplied_update_metahandles->erase(handle);
          DCHECK_EQ(entry->ref(IS_UNAPPLIED_UPDATE), num_erased > 0);
          num_erased = kernel_->parent_id_child_index->erase(entry);
          DCHECK_EQ(entry->ref(IS_DEL), !num_erased);
          kernel_->metahandles_index->erase(it++);
          delete entry;
        } else {
          ++it;
        }
      }

      // Ensure meta tracking for these data types reflects the deleted state.
      for (std::set<ModelType>::const_iterator it = types.begin();
           it != types.end(); ++it) {
        set_initial_sync_ended_for_type_unsafe(*it, false);
        kernel_->persisted_info.reset_download_progress(*it);
      }
    }
  }
}

void Directory::HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot) {
  ScopedKernelLock lock(this);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;

  // Because we optimistically cleared the dirty bit on the real entries when
  // taking the snapshot, we must restore it on failure.  Not doing this could
  // cause lost data, if no other changes are made to the in-memory entries
  // that would cause the dirty bit to get set again. Setting the bit ensures
  // that SaveChanges will at least try again later.
  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    kernel_->needle.put(META_HANDLE, i->ref(META_HANDLE));
    MetahandlesIndex::iterator found =
        kernel_->metahandles_index->find(&kernel_->needle);
    if (found != kernel_->metahandles_index->end()) {
      (*found)->mark_dirty(kernel_->dirty_metahandles);
    }
  }

  kernel_->metahandles_to_purge->insert(snapshot.metahandles_to_purge.begin(),
                                        snapshot.metahandles_to_purge.end());
}

void Directory::GetDownloadProgress(
    ModelType model_type,
    sync_pb::DataTypeProgressMarker* value_out) const {
  ScopedKernelLock lock(this);
  return value_out->CopyFrom(
      kernel_->persisted_info.download_progress[model_type]);
}

void Directory::GetDownloadProgressAsString(
    ModelType model_type,
    std::string* value_out) const {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].SerializeToString(
      value_out);
}

size_t Directory::GetEntriesCount() const {
  ScopedKernelLock lock(this);
  return kernel_->metahandles_index ? kernel_->metahandles_index->size() : 0;
}

void Directory::SetDownloadProgress(
    ModelType model_type,
    const sync_pb::DataTypeProgressMarker& new_progress) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].CopyFrom(new_progress);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

bool Directory::initial_sync_ended_for_type(ModelType type) const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.initial_sync_ended[type];
}

template <class T> void Directory::TestAndSet(
    T* kernel_data, const T* data_to_set) {
  if (*kernel_data != *data_to_set) {
    *kernel_data = *data_to_set;
    kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  }
}

void Directory::set_initial_sync_ended_for_type(ModelType type, bool x) {
  ScopedKernelLock lock(this);
  set_initial_sync_ended_for_type_unsafe(type, x);
}

void Directory::set_initial_sync_ended_for_type_unsafe(ModelType type,
                                                       bool x) {
  if (kernel_->persisted_info.initial_sync_ended[type] == x)
    return;
  kernel_->persisted_info.initial_sync_ended.set(type, x);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

void Directory::SetNotificationStateUnsafe(
    const std::string& notification_state) {
  if (notification_state == kernel_->persisted_info.notification_state)
    return;
  kernel_->persisted_info.notification_state = notification_state;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

string Directory::store_birthday() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.store_birthday;
}

void Directory::set_store_birthday(const string& store_birthday) {
  ScopedKernelLock lock(this);
  if (kernel_->persisted_info.store_birthday == store_birthday)
    return;
  kernel_->persisted_info.store_birthday = store_birthday;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

std::string Directory::GetNotificationState() const {
  ScopedKernelLock lock(this);
  std::string notification_state = kernel_->persisted_info.notification_state;
  return notification_state;
}

void Directory::SetNotificationState(const std::string& notification_state) {
  ScopedKernelLock lock(this);
  SetNotificationStateUnsafe(notification_state);
}

string Directory::cache_guid() const {
  // No need to lock since nothing ever writes to it after load.
  return kernel_->cache_guid;
}

void Directory::GetAllMetaHandles(BaseTransaction* trans,
                                  MetahandleSet* result) {
  result->clear();
  ScopedKernelLock lock(this);
  MetahandlesIndex::iterator i;
  for (i = kernel_->metahandles_index->begin();
       i != kernel_->metahandles_index->end();
       ++i) {
    result->insert((*i)->ref(META_HANDLE));
  }
}

void Directory::GetAllEntryKernels(BaseTransaction* trans,
                                   std::vector<const EntryKernel*>* result) {
  result->clear();
  ScopedKernelLock lock(this);
  result->insert(result->end(),
                 kernel_->metahandles_index->begin(),
                 kernel_->metahandles_index->end());
}

void Directory::GetUnsyncedMetaHandles(BaseTransaction* trans,
                                       UnsyncedMetaHandles* result) {
  result->clear();
  ScopedKernelLock lock(this);
  copy(kernel_->unsynced_metahandles->begin(),
       kernel_->unsynced_metahandles->end(), back_inserter(*result));
}

int64 Directory::unsynced_entity_count() const {
  ScopedKernelLock lock(this);
  return kernel_->unsynced_metahandles->size();
}

void Directory::GetUnappliedUpdateMetaHandles(BaseTransaction* trans,
    UnappliedUpdateMetaHandles* result) {
  result->clear();
  ScopedKernelLock lock(this);
  copy(kernel_->unapplied_update_metahandles->begin(),
       kernel_->unapplied_update_metahandles->end(),
       back_inserter(*result));
}


class IdFilter {
 public:
  virtual ~IdFilter() { }
  virtual bool ShouldConsider(const Id& id) const = 0;
};


class FullScanFilter : public IdFilter {
 public:
  virtual bool ShouldConsider(const Id& id) const {
    return true;
  }
};

class SomeIdsFilter : public IdFilter {
 public:
  virtual bool ShouldConsider(const Id& id) const {
    return binary_search(ids_.begin(), ids_.end(), id);
  }
  std::vector<Id> ids_;
};

void Directory::CheckTreeInvariants(syncable::BaseTransaction* trans,
                                    const EntryKernelMutationMap& mutations) {
  MetahandleSet handles;
  SomeIdsFilter filter;
  filter.ids_.reserve(mutations.size());
  for (EntryKernelMutationMap::const_iterator it = mutations.begin(),
         end = mutations.end(); it != end; ++it) {
    filter.ids_.push_back(it->second.mutated.ref(ID));
    handles.insert(it->first);
  }
  std::sort(filter.ids_.begin(), filter.ids_.end());
  CheckTreeInvariants(trans, handles, filter);
}

void Directory::CheckTreeInvariants(syncable::BaseTransaction* trans,
                                    bool full_scan) {
  // TODO(timsteele):  This is called every time a WriteTransaction finishes.
  // The performance hit is substantial given that we now examine every single
  // syncable entry.  Need to redesign this.
  MetahandleSet handles;
  GetAllMetaHandles(trans, &handles);
  if (full_scan) {
    FullScanFilter fullfilter;
    CheckTreeInvariants(trans, handles, fullfilter);
  } else {
    SomeIdsFilter filter;
    MetahandleSet::iterator i;
    for (i = handles.begin() ; i != handles.end() ; ++i) {
      Entry e(trans, GET_BY_HANDLE, *i);
      CHECK(e.good());
      filter.ids_.push_back(e.Get(ID));
    }
    sort(filter.ids_.begin(), filter.ids_.end());
    CheckTreeInvariants(trans, handles, filter);
  }
}

void Directory::CheckTreeInvariants(syncable::BaseTransaction* trans,
                                    const MetahandleSet& handles,
                                    const IdFilter& idfilter) {
  const int64 max_ms = kInvariantCheckMaxMs;
  PerfTimer check_timer;
  MetahandleSet::const_iterator i;
  int entries_done = 0;
  for (i = handles.begin() ; i != handles.end() ; ++i) {
    int64 metahandle = *i;
    Entry e(trans, GET_BY_HANDLE, metahandle);
    CHECK(e.good());
    syncable::Id id = e.Get(ID);
    syncable::Id parentid = e.Get(PARENT_ID);

    if (id.IsRoot()) {
      CHECK(e.Get(IS_DIR)) << e;
      CHECK(parentid.IsRoot()) << e;
      CHECK(!e.Get(IS_UNSYNCED)) << e;
      ++entries_done;
      continue;
    }

    if (!e.Get(IS_DEL)) {
      CHECK(id != parentid) << e;
      CHECK(!e.Get(NON_UNIQUE_NAME).empty()) << e;
      int safety_count = handles.size() + 1;
      while (!parentid.IsRoot()) {
        if (!idfilter.ShouldConsider(parentid))
          break;
        Entry parent(trans, GET_BY_ID, parentid);
        CHECK(parent.good()) << e;
        CHECK(parent.Get(IS_DIR)) << parent << e;
        CHECK(!parent.Get(IS_DEL)) << parent << e;
        CHECK(handles.end() != handles.find(parent.Get(META_HANDLE)))
            << e << parent;
        parentid = parent.Get(PARENT_ID);
        CHECK_GE(--safety_count, 0) << e << parent;
      }
    }
    int64 base_version = e.Get(BASE_VERSION);
    int64 server_version = e.Get(SERVER_VERSION);
    bool using_unique_client_tag = !e.Get(UNIQUE_CLIENT_TAG).empty();
    if (CHANGES_VERSION == base_version || 0 == base_version) {
      if (e.Get(IS_UNAPPLIED_UPDATE)) {
        // Must be a new item, or a de-duplicated unique client tag
        // that was created both locally and remotely.
        if (!using_unique_client_tag) {
          CHECK(e.Get(IS_DEL)) << e;
        }
        // It came from the server, so it must have a server ID.
        CHECK(id.ServerKnows()) << e;
      } else {
        if (e.Get(IS_DIR)) {
          // TODO(chron): Implement this mode if clients ever need it.
          // For now, you can't combine a client tag and a directory.
          CHECK(!using_unique_client_tag) << e;
        }
        // Should be an uncomitted item, or a successfully deleted one.
        if (!e.Get(IS_DEL)) {
          CHECK(e.Get(IS_UNSYNCED)) << e;
        }
        // If the next check failed, it would imply that an item exists
        // on the server, isn't waiting for application locally, but either
        // is an unsynced create or a sucessful delete in the local copy.
        // Either way, that's a mismatch.
        CHECK_EQ(0, server_version) << e;
        // Items that aren't using the unique client tag should have a zero
        // base version only if they have a local ID.  Items with unique client
        // tags are allowed to use the zero base version for undeletion and
        // de-duplication; the unique client tag trumps the server ID.
        if (!using_unique_client_tag) {
          CHECK(!id.ServerKnows()) << e;
        }
      }
    } else {
      CHECK(id.ServerKnows());
    }
    ++entries_done;
    int64 elapsed_ms = check_timer.Elapsed().InMilliseconds();
    if (elapsed_ms > max_ms) {
      VLOG(1) << "Cutting Invariant check short after " << elapsed_ms
              << "ms. Processed " << entries_done << "/" << handles.size()
              << " entries";
      return;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// ScopedKernelLock

ScopedKernelLock::ScopedKernelLock(const Directory* dir)
  :  scoped_lock_(dir->kernel_->mutex), dir_(const_cast<Directory*>(dir)) {
}

///////////////////////////////////////////////////////////////////////////
// Transactions

void BaseTransaction::Lock() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  dirkernel_->transaction_mutex.Acquire();

  time_acquired_ = base::TimeTicks::Now();
  const base::TimeDelta elapsed = time_acquired_ - start_time;
  VLOG_LOC(from_here_, 2)
      << name_ << " transaction waited "
      << elapsed.InSecondsF() << " seconds.";
}

void BaseTransaction::Unlock() {
  dirkernel_->transaction_mutex.Release();
  const base::TimeDelta elapsed = base::TimeTicks::Now() - time_acquired_;
  VLOG_LOC(from_here_, 2)
        << name_ << " transaction completed in " << elapsed.InSecondsF()
        << " seconds.";
}

BaseTransaction::BaseTransaction(const tracked_objects::Location& from_here,
                                 const char* name,
                                 WriterTag writer,
                                 Directory* directory)
    : from_here_(from_here), name_(name), writer_(writer),
      directory_(directory), dirkernel_(directory->kernel_) {
  dirkernel_->transaction_observer.Call(FROM_HERE,
      &TransactionObserver::OnTransactionStart, from_here_, writer_);
}

BaseTransaction::~BaseTransaction() {
  if (writer_ != INVALID) {
    dirkernel_->transaction_observer.Call(FROM_HERE,
        &TransactionObserver::OnTransactionEnd, from_here_, writer_);
  }
}

ReadTransaction::ReadTransaction(const tracked_objects::Location& location,
                                 Directory* directory)
    : BaseTransaction(location, "Read", INVALID, directory) {
  Lock();
}

ReadTransaction::ReadTransaction(const tracked_objects::Location& location,
                                 const ScopedDirLookup& scoped_dir)
    : BaseTransaction(location, "Read", INVALID, scoped_dir.operator->()) {
  Lock();
}

ReadTransaction::~ReadTransaction() {
  Unlock();
}

WriteTransaction::WriteTransaction(const tracked_objects::Location& location,
                                   WriterTag writer, Directory* directory)
    : BaseTransaction(location, "Write", writer, directory) {
  Lock();
}

WriteTransaction::WriteTransaction(const tracked_objects::Location& location,
                                   WriterTag writer,
                                   const ScopedDirLookup& scoped_dir)
    : BaseTransaction(location, "Write", writer, scoped_dir.operator->()) {
  Lock();
}

void WriteTransaction::SaveOriginal(const EntryKernel* entry) {
  if (!entry) {
    return;
  }
  // Insert only if it's not already there.
  const int64 handle = entry->ref(META_HANDLE);
  EntryKernelMutationMap::iterator it = mutations_.lower_bound(handle);
  if (it == mutations_.end() || it->first != handle) {
    EntryKernelMutation mutation;
    mutation.original = *entry;
    ignore_result(mutations_.insert(it, std::make_pair(handle, mutation)));
  }
}

ImmutableEntryKernelMutationMap WriteTransaction::RecordMutations() {
  dirkernel_->transaction_mutex.AssertAcquired();
  for (syncable::EntryKernelMutationMap::iterator it = mutations_.begin();
       it != mutations_.end();) {
    EntryKernel* kernel = directory()->GetEntryByHandle(it->first);
    if (!kernel) {
      NOTREACHED();
      continue;
    }
    if (kernel->is_dirty()) {
      it->second.mutated = *kernel;
      ++it;
    } else {
      DCHECK(!it->second.original.is_dirty());
      // Not actually mutated, so erase from |mutations_|.
      mutations_.erase(it++);
    }
  }
  return ImmutableEntryKernelMutationMap(&mutations_);
}

void WriteTransaction::UnlockAndNotify(
    const ImmutableEntryKernelMutationMap& mutations) {
  // Work while transaction mutex is held.
  ModelTypeBitSet models_with_changes;
  bool has_mutations = !mutations.Get().empty();
  if (has_mutations) {
    models_with_changes = NotifyTransactionChangingAndEnding(mutations);
  }
  Unlock();

  // Work after mutex is relased.
  if (has_mutations) {
    NotifyTransactionComplete(models_with_changes);
  }
}

ModelTypeBitSet WriteTransaction::NotifyTransactionChangingAndEnding(
    const ImmutableEntryKernelMutationMap& mutations) {
  dirkernel_->transaction_mutex.AssertAcquired();
  DCHECK(!mutations.Get().empty());

  WriteTransactionInfo write_transaction_info(
      dirkernel_->next_write_transaction_id, from_here_, writer_, mutations);
  ++dirkernel_->next_write_transaction_id;

  ImmutableWriteTransactionInfo immutable_write_transaction_info(
      &write_transaction_info);
  DirectoryChangeDelegate* const delegate = dirkernel_->delegate;
  if (writer_ == syncable::SYNCAPI) {
    delegate->HandleCalculateChangesChangeEventFromSyncApi(
        immutable_write_transaction_info, this);
  } else {
    delegate->HandleCalculateChangesChangeEventFromSyncer(
        immutable_write_transaction_info, this);
  }

  ModelTypeBitSet models_with_changes =
      delegate->HandleTransactionEndingChangeEvent(
          immutable_write_transaction_info, this);

  dirkernel_->transaction_observer.Call(FROM_HERE,
      &TransactionObserver::OnTransactionWrite,
      immutable_write_transaction_info, models_with_changes);

  return models_with_changes;
}

void WriteTransaction::NotifyTransactionComplete(
    ModelTypeBitSet models_with_changes) {
  dirkernel_->delegate->HandleTransactionCompleteChangeEvent(
      models_with_changes);
}

WriteTransaction::~WriteTransaction() {
  const ImmutableEntryKernelMutationMap& mutations = RecordMutations();

  if (OFF != kInvariantCheckLevel) {
    const bool full_scan = (FULL_DB_VERIFICATION == kInvariantCheckLevel);
    if (full_scan)
      directory()->CheckTreeInvariants(this, full_scan);
    else
      directory()->CheckTreeInvariants(this, mutations.Get());
  }

  UnlockAndNotify(mutations);
}

///////////////////////////////////////////////////////////////////////////
// Entry

Entry::Entry(BaseTransaction* trans, GetById, const Id& id)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryById(id);
}

Entry::Entry(BaseTransaction* trans, GetByClientTag, const string& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByClientTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetByServerTag, const string& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByServerTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetByHandle, int64 metahandle)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByHandle(metahandle);
}

Directory* Entry::dir() const {
  return basetrans_->directory();
}

Id Entry::ComputePrevIdFromServerPosition(const Id& parent_id) const {
  return dir()->ComputePrevIdFromServerPosition(kernel_, parent_id);
}

DictionaryValue* Entry::ToValue() const {
  DictionaryValue* entry_info = new DictionaryValue();
  entry_info->SetBoolean("good", good());
  if (good()) {
    entry_info->Set("kernel", kernel_->ToValue());
    entry_info->Set("serverModelType",
                    ModelTypeToValue(GetServerModelTypeHelper()));
    entry_info->Set("modelType",
                    ModelTypeToValue(GetModelType()));
    entry_info->SetBoolean("existsOnClientBecauseNameIsNonEmpty",
                           ExistsOnClientBecauseNameIsNonEmpty());
    entry_info->SetBoolean("isRoot", IsRoot());
  }
  return entry_info;
}

const string& Entry::Get(StringField field) const {
  DCHECK(kernel_);
  return kernel_->ref(field);
}

syncable::ModelType Entry::GetServerModelType() const {
  ModelType specifics_type = GetServerModelTypeHelper();
  if (specifics_type != UNSPECIFIED)
    return specifics_type;

  // Otherwise, we don't have a server type yet.  That should only happen
  // if the item is an uncommitted locally created item.
  // It's possible we'll need to relax these checks in the future; they're
  // just here for now as a safety measure.
  DCHECK(Get(IS_UNSYNCED));
  DCHECK_EQ(Get(SERVER_VERSION), 0);
  DCHECK(Get(SERVER_IS_DEL));
  // Note: can't enforce !Get(ID).ServerKnows() here because that could
  // actually happen if we hit AttemptReuniteLostCommitResponses.
  return UNSPECIFIED;
}

syncable::ModelType Entry::GetServerModelTypeHelper() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(Get(SERVER_SPECIFICS));
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!Get(UNIQUE_SERVER_TAG).empty() && Get(SERVER_IS_DIR))
    return TOP_LEVEL_FOLDER;

  return UNSPECIFIED;
}

syncable::ModelType Entry::GetModelType() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(Get(SPECIFICS));
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!Get(UNIQUE_SERVER_TAG).empty() && Get(IS_DIR))
    return TOP_LEVEL_FOLDER;

  return UNSPECIFIED;
}

///////////////////////////////////////////////////////////////////////////
// MutableEntry

MutableEntry::MutableEntry(WriteTransaction* trans, Create,
                           const Id& parent_id, const string& name)
    : Entry(trans),
      write_transaction_(trans) {
  Init(trans, parent_id, name);
}


void MutableEntry::Init(WriteTransaction* trans, const Id& parent_id,
                        const string& name) {
  kernel_ = new EntryKernel();
  kernel_->put(ID, trans->directory_->NextId());
  kernel_->put(META_HANDLE, trans->directory_->NextMetahandle());
  kernel_->mark_dirty(trans->directory_->kernel_->dirty_metahandles);
  kernel_->put(PARENT_ID, parent_id);
  kernel_->put(NON_UNIQUE_NAME, name);
  const base::Time& now = base::Time::Now();
  kernel_->put(CTIME, now);
  kernel_->put(MTIME, now);
  // We match the database defaults here
  kernel_->put(BASE_VERSION, CHANGES_VERSION);
  trans->directory()->InsertEntry(kernel_);
  // Because this entry is new, it was originally deleted.
  kernel_->put(IS_DEL, true);
  trans->SaveOriginal(kernel_);
  kernel_->put(IS_DEL, false);
}

MutableEntry::MutableEntry(WriteTransaction* trans, CreateNewUpdateItem,
                           const Id& id)
    : Entry(trans), write_transaction_(trans) {
  Entry same_id(trans, GET_BY_ID, id);
  if (same_id.good()) {
    kernel_ = NULL;  // already have an item with this ID.
    return;
  }
  kernel_ = new EntryKernel();
  kernel_->put(ID, id);
  kernel_->put(META_HANDLE, trans->directory_->NextMetahandle());
  kernel_->mark_dirty(trans->directory_->kernel_->dirty_metahandles);
  kernel_->put(IS_DEL, true);
  // We match the database defaults here
  kernel_->put(BASE_VERSION, CHANGES_VERSION);
  trans->directory()->InsertEntry(kernel_);
  trans->SaveOriginal(kernel_);
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetById, const Id& id)
    : Entry(trans, GET_BY_ID, id), write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByHandle,
                           int64 metahandle)
    : Entry(trans, GET_BY_HANDLE, metahandle), write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByClientTag,
                           const std::string& tag)
    : Entry(trans, GET_BY_CLIENT_TAG, tag), write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByServerTag,
                           const string& tag)
    : Entry(trans, GET_BY_SERVER_TAG, tag), write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

bool MutableEntry::PutIsDel(bool is_del) {
  DCHECK(kernel_);
  if (is_del == kernel_->ref(IS_DEL)) {
    return true;
  }
  if (is_del)
    UnlinkFromOrder();

  {
    ScopedKernelLock lock(dir());
    // Some indices don't include deleted items and must be updated
    // upon a value change.
    ScopedIndexUpdater<ParentIdAndHandleIndexer> updater(lock, kernel_,
        dir()->kernel_->parent_id_child_index);

    kernel_->put(IS_DEL, is_del);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }

  if (!is_del)
    // Restores position to the 0th index.
    if (!PutPredecessor(Id())) {
      // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
      NOTREACHED();
    }

  return true;
}

bool MutableEntry::Put(Int64Field field, const int64& value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    ScopedKernelLock lock(dir());
    if (SERVER_POSITION_IN_PARENT == field) {
      ScopedIndexUpdater<ParentIdAndHandleIndexer> updater(lock, kernel_,
          dir()->kernel_->parent_id_child_index);
      kernel_->put(field, value);
    } else {
      kernel_->put(field, value);
    }
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(TimeField field, const base::Time& value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(IdField field, const Id& value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    if (ID == field) {
      if (!dir()->ReindexId(kernel_, value))
        return false;
    } else if (PARENT_ID == field) {
      PutParentIdPropertyOnly(value);  // Makes sibling order inconsistent.
      // Fixes up the sibling order inconsistency.
      if (!PutPredecessor(Id())) {
        // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
        NOTREACHED();
      }
    } else {
      kernel_->put(field, value);
    }
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void MutableEntry::PutParentIdPropertyOnly(const Id& parent_id) {
  dir()->ReindexParentId(kernel_, parent_id);
  kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
}

bool MutableEntry::Put(BaseVersion field, int64 value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(StringField field, const string& value) {
  return PutImpl(field, value);
}

bool MutableEntry::Put(ProtoField field,
                       const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(field).SerializeAsString() != value.SerializeAsString()) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(BitField field, bool value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }
  return true;
}

MetahandleSet* MutableEntry::GetDirtyIndexHelper() {
  return dir()->kernel_->dirty_metahandles;
}

bool MutableEntry::PutUniqueClientTag(const string& new_tag) {
  // There is no SERVER_UNIQUE_CLIENT_TAG. This field is similar to ID.
  string old_tag = kernel_->ref(UNIQUE_CLIENT_TAG);
  if (old_tag == new_tag) {
    return true;
  }

  ScopedKernelLock lock(dir());
  if (!new_tag.empty()) {
    // Make sure your new value is not in there already.
    EntryKernel lookup_kernel_ = *kernel_;
    lookup_kernel_.put(UNIQUE_CLIENT_TAG, new_tag);
    bool new_tag_conflicts =
        (dir()->kernel_->client_tag_index->count(&lookup_kernel_) > 0);
    if (new_tag_conflicts) {
      return false;
    }
  }

  {
    ScopedIndexUpdater<ClientTagIndexer> index_updater(lock, kernel_,
        dir()->kernel_->client_tag_index);
    kernel_->put(UNIQUE_CLIENT_TAG, new_tag);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::PutImpl(StringField field, const string& value) {
  DCHECK(kernel_);
  if (field == UNIQUE_CLIENT_TAG) {
    return PutUniqueClientTag(value);
  }

  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(IndexedBitField field, bool value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    MetahandleSet* index;
    if (IS_UNSYNCED == field)
      index = dir()->kernel_->unsynced_metahandles;
    else
      index = dir()->kernel_->unapplied_update_metahandles;

    ScopedKernelLock lock(dir());
    if (value)
      CHECK(index->insert(kernel_->ref(META_HANDLE)).second);
    else
      CHECK_EQ(1U, index->erase(kernel_->ref(META_HANDLE)));
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void MutableEntry::UnlinkFromOrder() {
  ScopedKernelLock lock(dir());
  dir()->UnlinkEntryFromOrder(kernel_, write_transaction(), &lock);
}

void Directory::UnlinkEntryFromOrder(EntryKernel* entry,
                                     WriteTransaction* trans,
                                     ScopedKernelLock* lock) {
  CHECK(!trans || this == trans->directory());
  Id old_previous = entry->ref(PREV_ID);
  Id old_next = entry->ref(NEXT_ID);

  entry->put(NEXT_ID, entry->ref(ID));
  entry->put(PREV_ID, entry->ref(ID));
  entry->mark_dirty(kernel_->dirty_metahandles);

  if (!old_previous.IsRoot()) {
    if (old_previous == old_next) {
      // Note previous == next doesn't imply previous == next == Get(ID). We
      // could have prev==next=="c-XX" and Get(ID)=="sX..." if an item was added
      // and deleted before receiving the server ID in the commit response.
      CHECK((old_next == entry->ref(ID)) || !old_next.ServerKnows());
      return;  // Done if we were already self-looped (hence unlinked).
    }
    EntryKernel* previous_entry = GetEntryById(old_previous, lock);
    CHECK(previous_entry);
    if (trans)
      trans->SaveOriginal(previous_entry);
    previous_entry->put(NEXT_ID, old_next);
    previous_entry->mark_dirty(kernel_->dirty_metahandles);
  }

  if (!old_next.IsRoot()) {
    EntryKernel* next_entry = GetEntryById(old_next, lock);
    CHECK(next_entry);
    if (trans)
      trans->SaveOriginal(next_entry);
    next_entry->put(PREV_ID, old_previous);
    next_entry->mark_dirty(kernel_->dirty_metahandles);
  }
}

bool MutableEntry::PutPredecessor(const Id& predecessor_id) {
  UnlinkFromOrder();

  if (Get(IS_DEL)) {
    DCHECK(predecessor_id.IsNull());
    return true;
  }

  // TODO(ncarter): It should be possible to not maintain position for
  // non-bookmark items.  However, we'd need to robustly handle all possible
  // permutations of setting IS_DEL and the SPECIFICS to identify the
  // object type; or else, we'd need to add a ModelType to the
  // MutableEntry's Create ctor.
  //   if (!ShouldMaintainPosition()) {
  //     return false;
  //   }

  // This is classic insert-into-doubly-linked-list from CS 101 and your last
  // job interview.  An "IsRoot" Id signifies the head or tail.
  Id successor_id;
  if (!predecessor_id.IsRoot()) {
    MutableEntry predecessor(write_transaction(), GET_BY_ID, predecessor_id);
    if (!predecessor.good()) {
      LOG(ERROR) << "Predecessor is not good : "
                 << predecessor_id.GetServerId();
      return false;
    }
    if (predecessor.Get(PARENT_ID) != Get(PARENT_ID))
      return false;
    successor_id = predecessor.Get(NEXT_ID);
    predecessor.Put(NEXT_ID, Get(ID));
  } else {
    syncable::Directory* dir = trans()->directory();
    if (!dir->GetFirstChildId(trans(), Get(PARENT_ID), &successor_id)) {
      return false;
    }
  }
  if (!successor_id.IsRoot()) {
    MutableEntry successor(write_transaction(), GET_BY_ID, successor_id);
    if (!successor.good()) {
      LOG(ERROR) << "Successor is not good: "
                 << successor_id.GetServerId();
      return false;
    }
    if (successor.Get(PARENT_ID) != Get(PARENT_ID))
      return false;
    successor.Put(PREV_ID, Get(ID));
  }
  DCHECK(predecessor_id != Get(ID));
  DCHECK(successor_id != Get(ID));
  Put(PREV_ID, predecessor_id);
  Put(NEXT_ID, successor_id);
  return true;
}

bool MutableEntry::Put(BitTemp field, bool value) {
  DCHECK(kernel_);
  kernel_->put(field, value);
  return true;
}

///////////////////////////////////////////////////////////////////////////
// High-level functions

int64 Directory::NextMetahandle() {
  ScopedKernelLock lock(this);
  int64 metahandle = (kernel_->next_metahandle)++;
  return metahandle;
}

// Always returns a client ID that is the string representation of a negative
// number.
Id Directory::NextId() {
  int64 result;
  {
    ScopedKernelLock lock(this);
    result = (kernel_->persisted_info.next_id)--;
    kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  }
  DCHECK_LT(result, 0);
  return Id::CreateFromClientString(base::Int64ToString(result));
}

bool Directory::HasChildren(BaseTransaction* trans, const Id& id) {
  ScopedKernelLock lock(this);
  return (GetPossibleFirstChild(lock, id) != NULL);
}

bool Directory::GetFirstChildId(BaseTransaction* trans,
                                const Id& parent_id,
                                Id* first_child_id) {
  ScopedKernelLock lock(this);
  EntryKernel* entry = GetPossibleFirstChild(lock, parent_id);
  if (!entry) {
    *first_child_id = Id();
    return true;
  }

  // Walk to the front of the list; the server position ordering
  // is commonly identical to the linked-list ordering, but pending
  // unsynced or unapplied items may diverge.
  while (!entry->ref(PREV_ID).IsRoot()) {
    entry = GetEntryById(entry->ref(PREV_ID), &lock);
    if (!entry) {
      *first_child_id = Id();
      return false;
    }
  }
  *first_child_id = entry->ref(ID);
  return true;
}

bool Directory::GetLastChildIdForTest(
    BaseTransaction* trans, const Id& parent_id, Id* last_child_id) {
  ScopedKernelLock lock(this);
  EntryKernel* entry = GetPossibleLastChildForTest(lock, parent_id);
  if (!entry) {
    *last_child_id = Id();
    return true;
  }

  // Walk to the back of the list; the server position ordering
  // is commonly identical to the linked-list ordering, but pending
  // unsynced or unapplied items may diverge.
  while (!entry->ref(NEXT_ID).IsRoot()) {
    entry = GetEntryById(entry->ref(NEXT_ID), &lock);
    if (!entry) {
      *last_child_id = Id();
      return false;
    }
  }

  *last_child_id = entry->ref(ID);
  return true;
}

Id Directory::ComputePrevIdFromServerPosition(
    const EntryKernel* entry,
    const syncable::Id& parent_id) {
  ScopedKernelLock lock(this);

  // Find the natural insertion point in the parent_id_child_index, and
  // work back from there, filtering out ineligible candidates.
  ParentIdChildIndex::iterator sibling = LocateInParentChildIndex(lock,
      parent_id, entry->ref(SERVER_POSITION_IN_PARENT), entry->ref(ID));
  ParentIdChildIndex::iterator first_sibling =
      GetParentChildIndexLowerBound(lock, parent_id);

  while (sibling != first_sibling) {
    --sibling;
    EntryKernel* candidate = *sibling;

    // The item itself should never be in the range under consideration.
    DCHECK_NE(candidate->ref(META_HANDLE), entry->ref(META_HANDLE));

    // Ignore unapplied updates -- they might not even be server-siblings.
    if (candidate->ref(IS_UNAPPLIED_UPDATE))
      continue;

    // We can't trust the SERVER_ fields of unsynced items, but they are
    // potentially legitimate local predecessors.  In the case where
    // |update_item| and an unsynced item wind up in the same insertion
    // position, we need to choose how to order them.  The following check puts
    // the unapplied update first; removing it would put the unsynced item(s)
    // first.
    if (candidate->ref(IS_UNSYNCED))
      continue;

    // Skip over self-looped items, which are not valid predecessors.  This
    // shouldn't happen in practice, but is worth defending against.
    if (candidate->ref(PREV_ID) == candidate->ref(NEXT_ID) &&
        !candidate->ref(PREV_ID).IsRoot()) {
      NOTREACHED();
      continue;
    }
    return candidate->ref(ID);
  }
  // This item will be the first in the sibling order.
  return Id();
}

bool IsLegalNewParent(BaseTransaction* trans, const Id& entry_id,
                      const Id& new_parent_id) {
  if (entry_id.IsRoot())
    return false;
  // we have to ensure that the entry is not an ancestor of the new parent.
  Id ancestor_id = new_parent_id;
  while (!ancestor_id.IsRoot()) {
    if (entry_id == ancestor_id)
      return false;
    Entry new_parent(trans, GET_BY_ID, ancestor_id);
    CHECK(new_parent.good());
    ancestor_id = new_parent.Get(PARENT_ID);
  }
  return true;
}

// This function sets only the flags needed to get this entry to sync.
void MarkForSyncing(syncable::MutableEntry* e) {
  DCHECK_NE(static_cast<MutableEntry*>(NULL), e);
  DCHECK(!e->IsRoot()) << "We shouldn't mark a permanent object for syncing.";
  e->Put(IS_UNSYNCED, true);
  e->Put(SYNCING, false);
}

std::ostream& operator<<(std::ostream& os, const Entry& entry) {
  int i;
  EntryKernel* const kernel = entry.kernel_;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << kernel->ref(static_cast<Int64Field>(i)) << ", ";
  }
  for ( ; i < TIME_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << browser_sync::GetTimeDebugString(
           kernel->ref(static_cast<TimeField>(i))) << ", ";
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << kernel->ref(static_cast<IdField>(i)) << ", ";
  }
  os << "Flags: ";
  for ( ; i < BIT_FIELDS_END; ++i) {
    if (kernel->ref(static_cast<BitField>(i)))
      os << g_metas_columns[i].name << ", ";
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    const string& field = kernel->ref(static_cast<StringField>(i));
    os << g_metas_columns[i].name << ": " << field << ", ";
  }
  for ( ; i < PROTO_FIELDS_END; ++i) {
    os << g_metas_columns[i].name << ": "
       << net::EscapePath(
              kernel->ref(static_cast<ProtoField>(i)).SerializeAsString())
       << ", ";
  }
  os << "TempFlags: ";
  for ( ; i < BIT_TEMPS_END; ++i) {
    if (kernel->ref(static_cast<BitTemp>(i)))
      os << "#" << i - BIT_TEMPS_BEGIN << ", ";
  }
  return os;
}

std::ostream& operator<<(std::ostream& s, const Blob& blob) {
  for (Blob::const_iterator i = blob.begin(); i != blob.end(); ++i)
    s << std::hex << std::setw(2)
      << std::setfill('0') << static_cast<unsigned int>(*i);
  return s << std::dec;
}

Directory::ParentIdChildIndex::iterator Directory::LocateInParentChildIndex(
    const ScopedKernelLock& lock,
    const Id& parent_id,
    int64 position_in_parent,
    const Id& item_id_for_tiebreaking) {
  kernel_->needle.put(PARENT_ID, parent_id);
  kernel_->needle.put(SERVER_POSITION_IN_PARENT, position_in_parent);
  kernel_->needle.put(ID, item_id_for_tiebreaking);
  return kernel_->parent_id_child_index->lower_bound(&kernel_->needle);
}

Directory::ParentIdChildIndex::iterator
Directory::GetParentChildIndexLowerBound(const ScopedKernelLock& lock,
                                         const Id& parent_id) {
  // Peg the parent ID, and use the least values for the remaining
  // index variables.
  return LocateInParentChildIndex(lock, parent_id,
      std::numeric_limits<int64>::min(),
      Id::GetLeastIdForLexicographicComparison());
}

Directory::ParentIdChildIndex::iterator
Directory::GetParentChildIndexUpperBound(const ScopedKernelLock& lock,
                                         const Id& parent_id) {
  // The upper bound of |parent_id|'s range is the lower
  // bound of |++parent_id|'s range.
  return GetParentChildIndexLowerBound(lock,
      parent_id.GetLexicographicSuccessor());
}

void Directory::AppendChildHandles(const ScopedKernelLock& lock,
                                   const Id& parent_id,
                                   Directory::ChildHandles* result) {
  typedef ParentIdChildIndex::iterator iterator;
  CHECK(result);
  for (iterator i = GetParentChildIndexLowerBound(lock, parent_id),
           end = GetParentChildIndexUpperBound(lock, parent_id);
       i != end; ++i) {
    DCHECK_EQ(parent_id, (*i)->ref(PARENT_ID));
    result->push_back((*i)->ref(META_HANDLE));
  }
}

EntryKernel* Directory::GetPossibleFirstChild(
    const ScopedKernelLock& lock, const Id& parent_id) {
  // We can use the server positional ordering as a hint because it's generally
  // in sync with the local (linked-list) positional ordering, and we have an
  // index on it.
  ParentIdChildIndex::iterator candidate =
      GetParentChildIndexLowerBound(lock, parent_id);
  ParentIdChildIndex::iterator end_range =
      GetParentChildIndexUpperBound(lock, parent_id);
  for (; candidate != end_range; ++candidate) {
    EntryKernel* entry = *candidate;
    // Filter out self-looped items, which are temporarily not in the child
    // ordering.
    if (entry->ref(PREV_ID).IsRoot() ||
        entry->ref(PREV_ID) != entry->ref(NEXT_ID)) {
      return entry;
    }
  }
  // There were no children in the linked list.
  return NULL;
}

EntryKernel* Directory::GetPossibleLastChildForTest(
    const ScopedKernelLock& lock, const Id& parent_id) {
  // We can use the server positional ordering as a hint because it's generally
  // in sync with the local (linked-list) positional ordering, and we have an
  // index on it.
  ParentIdChildIndex::iterator begin_range =
      GetParentChildIndexLowerBound(lock, parent_id);
  ParentIdChildIndex::iterator candidate =
      GetParentChildIndexUpperBound(lock, parent_id);

  while (begin_range != candidate) {
    --candidate;
    EntryKernel* entry = *candidate;

    // Filter out self-looped items, which are temporarily not in the child
    // ordering.
    if (entry->ref(NEXT_ID).IsRoot() ||
        entry->ref(NEXT_ID) != entry->ref(PREV_ID)) {
      return entry;
    }
  }
  // There were no children in the linked list.
  return NULL;
}

}  // namespace syncable
