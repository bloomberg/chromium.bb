// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable.h"

#include "build/build_config.h"

#include <sys/stat.h>
#if defined(OS_POSIX)
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <time.h>
#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(OS_WIN)
#include <shlwapi.h>  // for PathMatchSpec
#endif

#include <algorithm>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <set>
#include <string>

#include "base/hash_tables.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/perftimer.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"
#include "chrome/browser/sync/syncable/syncable_columns.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/common/deprecated/event_sys-inl.h"
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

using browser_sync::SyncerUtil;
using std::string;


namespace syncable {

int64 Now() {
#if defined(OS_WIN)
  FILETIME filetime;
  SYSTEMTIME systime;
  GetSystemTime(&systime);
  SystemTimeToFileTime(&systime, &filetime);
  // MSDN recommends converting via memcpy like this.
  LARGE_INTEGER n;
  memcpy(&n, &filetime, sizeof(filetime));
  return n.QuadPart;
#elif defined(OS_POSIX)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<int64>(tv.tv_sec);
#else
#error NEED OS SPECIFIC Now() implementation
#endif
}

///////////////////////////////////////////////////////////////////////////
// Compare functions and hashes for the indices.

// Callback for sqlite3
// TODO(chron): This should be somewhere else
int ComparePathNames16(void*, int a_bytes, const void* a, int b_bytes,
                       const void* b) {
  int result = base::strncasecmp(reinterpret_cast<const char *>(a),
                                 reinterpret_cast<const char *>(b),
                                 std::min(a_bytes, b_bytes));
  if (result != 0) {
    return result;
  } else {
    return a_bytes > b_bytes ? 1 : b_bytes > a_bytes ? -1 : 0;
  }
}

template <Int64Field field_index>
class SameField {
 public:
  inline bool operator()(const syncable::EntryKernel* a,
                         const syncable::EntryKernel* b) const {
    return a->ref(field_index) == b->ref(field_index);
  }
};

template <Int64Field field_index>
class HashField {
 public:
  inline size_t operator()(const syncable::EntryKernel* a) const {
    return hasher_(a->ref(field_index));
  }
  base::hash_set<int64> hasher_;
};

// TODO(chron): Remove this function.
int ComparePathNames(const string& a, const string& b) {
  const size_t val_size = sizeof(string::value_type);
  return ComparePathNames16(NULL, a.size() * val_size, a.data(),
                                  b.size() * val_size, b.data());
}

class LessParentIdAndHandle {
 public:
  bool operator() (const syncable::EntryKernel* a,
                   const syncable::EntryKernel* b) const {
    if (a->ref(PARENT_ID) != b->ref(PARENT_ID)) {
      return a->ref(PARENT_ID) < b->ref(PARENT_ID);
    }

    // Meta handles are immutable per entry so this is ideal.
    return a->ref(META_HANDLE) < b->ref(META_HANDLE);
  }
};

// TODO(chron): Remove this function.
bool LessPathNames::operator() (const string& a, const string& b) const {
  return ComparePathNames(a, b) < 0;
}

///////////////////////////////////////////////////////////////////////////
// EntryKernel

EntryKernel::EntryKernel() : dirty_(false) {
  memset(int64_fields, 0, sizeof(int64_fields));
}

EntryKernel::~EntryKernel() {}

///////////////////////////////////////////////////////////////////////////
// Directory

static const DirectoryChangeEvent kShutdownChangesEvent =
    { DirectoryChangeEvent::SHUTDOWN, 0, 0 };

void Directory::init_kernel(const std::string& name) {
  DCHECK(kernel_ == NULL);
  kernel_ = new Kernel(FilePath(), name, KernelLoadInfo());
}

Directory::PersistedKernelInfo::PersistedKernelInfo()
    : next_id(0) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    reset_download_progress(ModelTypeFromInt(i));
  }
  autofill_migration_state = NOT_DETERMINED;
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

Directory::Kernel::Kernel(const FilePath& db_path,
                          const string& name,
                          const KernelLoadInfo& info)
    : db_path(db_path),
      refcount(1),
      name(name),
      metahandles_index(new Directory::MetahandlesIndex),
      ids_index(new Directory::IdsIndex),
      parent_id_child_index(new Directory::ParentIdChildIndex),
      client_tag_index(new Directory::ClientTagIndex),
      unapplied_update_metahandles(new MetahandleSet),
      unsynced_metahandles(new MetahandleSet),
      dirty_metahandles(new MetahandleSet),
      metahandles_to_purge(new MetahandleSet),
      channel(new Directory::Channel(syncable::DIRECTORY_DESTROYED)),
      info_status(Directory::KERNEL_SHARE_INFO_VALID),
      persisted_info(info.kernel_info),
      cache_guid(info.cache_guid),
      next_metahandle(info.max_metahandle + 1) {
}

void Directory::Kernel::AddRef() {
  base::subtle::NoBarrier_AtomicIncrement(&refcount, 1);
}

void Directory::Kernel::Release() {
  if (!base::subtle::NoBarrier_AtomicIncrement(&refcount, -1))
    delete this;
}

Directory::Kernel::~Kernel() {
  CHECK(0 == refcount);
  delete channel;
  changes_channel.Notify(kShutdownChangesEvent);
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

DirOpenResult Directory::Open(const FilePath& file_path, const string& name) {
  const DirOpenResult result = OpenImpl(file_path, name);
  if (OPENED != result)
    Close();
  return result;
}

void Directory::InitializeIndices() {
  MetahandlesIndex::iterator it = kernel_->metahandles_index->begin();
  for (; it != kernel_->metahandles_index->end(); ++it) {
    EntryKernel* entry = *it;
    if (!entry->ref(IS_DEL))
      kernel_->parent_id_child_index->insert(entry);
    kernel_->ids_index->insert(entry);
    if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
      kernel_->client_tag_index->insert(entry);
    }
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

DirOpenResult Directory::OpenImpl(const FilePath& file_path,
                                  const string& name) {
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

  kernel_ = new Kernel(db_path, name, info);
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

// An interface to specify the details of which children
// GetChildHandles() is looking for.
// TODO(chron): Clean this up into one function to get child handles
struct PathMatcher {
  explicit PathMatcher(const Id& parent_id) : parent_id_(parent_id) { }
  virtual ~PathMatcher() { }

  typedef Directory::ParentIdChildIndex Index;
  virtual Index::iterator lower_bound(Index* index) = 0;
  virtual Index::iterator upper_bound(Index* index) = 0;
  const Id parent_id_;
  EntryKernel needle_;
};

// Matches all children.
// TODO(chron): Unit test this by itself
struct AllPathsMatcher : public PathMatcher {
  explicit AllPathsMatcher(const Id& parent_id) : PathMatcher(parent_id) {
  }

  virtual Index::iterator lower_bound(Index* index) {
    needle_.put(PARENT_ID, parent_id_);
    needle_.put(META_HANDLE, std::numeric_limits<int64>::min());
    return index->lower_bound(&needle_);
  }

  virtual Index::iterator upper_bound(Index* index) {
    needle_.put(PARENT_ID, parent_id_);
    needle_.put(META_HANDLE, std::numeric_limits<int64>::max());
    return index->upper_bound(&needle_);
  }
};

void Directory::GetChildHandles(BaseTransaction* trans, const Id& parent_id,
                                Directory::ChildHandles* result) {
  AllPathsMatcher matcher(parent_id);
  return GetChildHandlesImpl(trans, parent_id, &matcher, result);
}

void Directory::GetChildHandlesImpl(BaseTransaction* trans, const Id& parent_id,
                                    PathMatcher* matcher,
                                    Directory::ChildHandles* result) {
  CHECK(this == trans->directory());
  result->clear();
  {
    ScopedKernelLock lock(this);

    // This index is sorted by parent id and metahandle.
    ParentIdChildIndex* const index = kernel_->parent_id_child_index;
    typedef ParentIdChildIndex::iterator iterator;
    for (iterator i = matcher->lower_bound(index),
           end = matcher->upper_bound(index); i != end; ++i) {
      // root's parent_id is NULL in the db but 0 in memory, so
      // have avoid listing the root as its own child.
      if ((*i)->ref(ID) == (*i)->ref(PARENT_ID))
        continue;
      result->push_back((*i)->ref(META_HANDLE));
    }
  }
}

EntryKernel* Directory::GetRootEntry() {
  return GetEntryById(Id());
}

void ZeroFields(EntryKernel* entry, int first_field) {
  int i = first_field;
  // Note that bitset<> constructor sets all bits to zero, and strings
  // initialize to empty.
  for ( ; i < INT64_FIELDS_END; ++i)
    entry->put(static_cast<Int64Field>(i), 0);
  for ( ; i < ID_FIELDS_END; ++i)
    entry->mutable_ref(static_cast<IdField>(i)).Clear();
  for ( ; i < BIT_FIELDS_END; ++i)
    entry->put(static_cast<BitField>(i), false);
  if (i < PROTO_FIELDS_END)
    i = PROTO_FIELDS_END;
  entry->clear_dirty(NULL);
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

void Directory::Undelete(EntryKernel* const entry) {
  DCHECK(entry->ref(IS_DEL));
  ScopedKernelLock lock(this);
  entry->put(IS_DEL, false);
  entry->mark_dirty(kernel_->dirty_metahandles);
  CHECK(kernel_->parent_id_child_index->insert(entry).second);
}

void Directory::Delete(EntryKernel* const entry) {
  DCHECK(!entry->ref(IS_DEL));
  entry->put(IS_DEL, true);
  entry->mark_dirty(kernel_->dirty_metahandles);
  ScopedKernelLock lock(this);
  CHECK(1 == kernel_->parent_id_child_index->erase(entry));
}

bool Directory::ReindexId(EntryKernel* const entry, const Id& new_id) {
  ScopedKernelLock lock(this);
  if (NULL != GetEntryById(new_id, &lock))
    return false;
  CHECK(1 == kernel_->ids_index->erase(entry));
  entry->put(ID, new_id);
  CHECK(kernel_->ids_index->insert(entry).second);
  return true;
}

void Directory::ReindexParentId(EntryKernel* const entry,
                                const Id& new_parent_id) {

  ScopedKernelLock lock(this);
  if (entry->ref(IS_DEL)) {
    entry->put(PARENT_ID, new_parent_id);
    return;
  }

  if (entry->ref(PARENT_ID) == new_parent_id) {
    return;
  }

  CHECK(1 == kernel_->parent_id_child_index->erase(entry));
  entry->put(PARENT_ID, new_parent_id);
  CHECK(kernel_->parent_id_child_index->insert(entry).second);
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
    CHECK(kernel_->dirty_metahandles->count(handle) == 0);
    // TODO(tim): Bug 49278.
    CHECK(!kernel_->unsynced_metahandles->count(handle));
    CHECK(!kernel_->unapplied_update_metahandles->count(handle));
  }

  return safe;
}

void Directory::TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot) {
  ReadTransaction trans(this, __FILE__, __LINE__);
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
  // Need a write transaction as we are about to permanently purge entries.
  WriteTransaction trans(this, VACUUM_AFTER_SAVE, __FILE__, __LINE__);
  ScopedKernelLock lock(this);
  kernel_->flushed_metahandles.Push(0);  // Begin flush marker
  // Now drop everything we can out of memory.
  for (OriginalEntries::const_iterator i = snapshot.dirty_metas.begin();
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
      int64 handle = entry->ref(META_HANDLE);
      kernel_->flushed_metahandles.Push(handle);
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
    WriteTransaction trans(this, PURGE_ENTRIES, __FILE__, __LINE__);
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
  for (OriginalEntries::const_iterator i = snapshot.dirty_metas.begin();
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

void Directory::SetDownloadProgress(
    ModelType model_type,
    const sync_pb::DataTypeProgressMarker& new_progress) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].CopyFrom(new_progress);
}

bool Directory::initial_sync_ended_for_type(ModelType type) const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.initial_sync_ended[type];
}

AutofillMigrationState Directory::get_autofill_migration_state() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.autofill_migration_state;
}

AutofillMigrationDebugInfo
    Directory::get_autofill_migration_debug_info() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.autofill_migration_debug_info;
}

template <class T> void Directory::TestAndSet(
    T* kernel_data, const T* data_to_set) {
  if (*kernel_data != *data_to_set) {
    *kernel_data = *data_to_set;
    kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  }
}

void Directory::set_autofill_migration_state_debug_info(
    AutofillMigrationDebugInfo::PropertyToSet property_to_set,
    const AutofillMigrationDebugInfo& info) {

  ScopedKernelLock lock(this);
  switch (property_to_set) {
    case AutofillMigrationDebugInfo::MIGRATION_TIME: {
      syncable::AutofillMigrationDebugInfo&
        debug_info = kernel_->persisted_info.autofill_migration_debug_info;
      TestAndSet<int64>(
          &debug_info.autofill_migration_time,
          &info.autofill_migration_time);
      break;
    }
    case AutofillMigrationDebugInfo::BOOKMARK_ADDED: {
      AutofillMigrationDebugInfo& debug_info =
        kernel_->persisted_info.autofill_migration_debug_info;
      TestAndSet<int>(
          &debug_info.bookmarks_added_during_migration,
          &info.bookmarks_added_during_migration);
      break;
    }
    case AutofillMigrationDebugInfo::ENTRIES_ADDED: {
      AutofillMigrationDebugInfo& debug_info =
        kernel_->persisted_info.autofill_migration_debug_info;
      TestAndSet<int>(
          &debug_info.autofill_entries_added_during_migration,
          &info.autofill_entries_added_during_migration);
      break;
    }
    case AutofillMigrationDebugInfo::PROFILES_ADDED: {
      AutofillMigrationDebugInfo& debug_info =
        kernel_->persisted_info.autofill_migration_debug_info;
      TestAndSet<int>(
          &debug_info.autofill_profile_added_during_migration,
          &info.autofill_profile_added_during_migration);
      break;
    }
    default:
      NOTREACHED();
  }
}

void Directory::set_autofill_migration_state(AutofillMigrationState state) {
  ScopedKernelLock lock(this);
  if (state == kernel_->persisted_info.autofill_migration_state) {
    return;
  }
  kernel_->persisted_info.autofill_migration_state = state;
  if (state == MIGRATED) {
    syncable::AutofillMigrationDebugInfo& debug_info =
        kernel_->persisted_info.autofill_migration_debug_info;
    debug_info.autofill_migration_time =
        base::Time::Now().ToInternalValue();
  }
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
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

void Directory::set_store_birthday(string store_birthday) {
  ScopedKernelLock lock(this);
  if (kernel_->persisted_info.store_birthday == store_birthday)
    return;
  kernel_->persisted_info.store_birthday = store_birthday;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

std::string Directory::GetAndClearNotificationState() {
  ScopedKernelLock lock(this);
  std::string notification_state = kernel_->persisted_info.notification_state;
  SetNotificationStateUnsafe(std::string());
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
                                    const OriginalEntries* originals) {
  MetahandleSet handles;
  SomeIdsFilter filter;
  filter.ids_.reserve(originals->size());
  for (OriginalEntries::const_iterator i = originals->begin(),
         end = originals->end(); i != end; ++i) {
    Entry e(trans, GET_BY_HANDLE, i->ref(META_HANDLE));
    CHECK(e.good());
    filter.ids_.push_back(e.Get(ID));
    handles.insert(i->ref(META_HANDLE));
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
  int64 max_ms = kInvariantCheckMaxMs;
  if (max_ms < 0)
    max_ms = std::numeric_limits<int64>::max();
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
        CHECK(--safety_count >= 0) << e << parent;
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
        CHECK(0 == server_version) << e;
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

browser_sync::ChannelHookup<DirectoryChangeEvent>* Directory::AddChangeObserver(
    browser_sync::ChannelEventHandler<DirectoryChangeEvent>* observer) {
  return kernel_->changes_channel.AddObserver(observer);
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
  if (LOG_IS_ON(INFO) &&
      (1 <= logging::GetVlogLevelHelper(
          source_file_, ::strlen(source_file_))) &&
      (elapsed.InMilliseconds() > 200)) {
    logging::LogMessage(source_file_, line_, logging::LOG_INFO).stream()
      << name_ << " transaction waited "
      << elapsed.InSecondsF() << " seconds.";
  }
}

BaseTransaction::BaseTransaction(Directory* directory, const char* name,
    const char* source_file, int line, WriterTag writer)
  : directory_(directory), dirkernel_(directory->kernel_), name_(name),
    source_file_(source_file), line_(line), writer_(writer) {
  Lock();
}

BaseTransaction::BaseTransaction(Directory* directory)
    : directory_(directory),
      dirkernel_(NULL),
      name_(NULL),
      source_file_(NULL),
      line_(0),
      writer_(INVALID) {
}

BaseTransaction::~BaseTransaction() {}

void BaseTransaction::UnlockAndLog(OriginalEntries* originals_arg) {
  // Triggers the CALCULATE_CHANGES and TRANSACTION_ENDING events while
  // holding dir_kernel_'s transaction_mutex and changes_channel mutex.
  // Releases all mutexes upon completion.
  if (!NotifyTransactionChangingAndEnding(originals_arg)) {
    return;
  }

  // Triggers the TRANSACTION_COMPLETE event (and does not hold any mutexes).
  NotifyTransactionComplete();
}

bool BaseTransaction::NotifyTransactionChangingAndEnding(
    OriginalEntries* originals_arg) {
  dirkernel_->transaction_mutex.AssertAcquired();

  scoped_ptr<OriginalEntries> originals(originals_arg);
  const base::TimeDelta elapsed = base::TimeTicks::Now() - time_acquired_;
  if (LOG_IS_ON(INFO) &&
      (1 <= logging::GetVlogLevelHelper(
          source_file_, ::strlen(source_file_))) &&
      (elapsed.InMilliseconds() > 50)) {
    logging::LogMessage(source_file_, line_, logging::LOG_INFO).stream()
        << name_ << " transaction completed in " << elapsed.InSecondsF()
        << " seconds.";
  }

  if (NULL == originals.get() || originals->empty()) {
    dirkernel_->transaction_mutex.Release();
    return false;
  }


  {
    // Scoped_lock is only active through the calculate_changes and
    // transaction_ending events.
    base::AutoLock scoped_lock(dirkernel_->changes_channel_mutex);

    // Tell listeners to calculate changes while we still have the mutex.
    DirectoryChangeEvent event = { DirectoryChangeEvent::CALCULATE_CHANGES,
                                   originals.get(), this, writer_ };
    dirkernel_->changes_channel.Notify(event);

    // Necessary for reads to be performed prior to transaction mutex release.
    // Allows the listener to use the current transaction to perform reads.
    DirectoryChangeEvent ending_event =
        { DirectoryChangeEvent::TRANSACTION_ENDING,
          NULL, this, INVALID };
    dirkernel_->changes_channel.Notify(ending_event);

    dirkernel_->transaction_mutex.Release();
  }

  return true;
}

void BaseTransaction::NotifyTransactionComplete() {
  // Transaction is no longer holding any locks/mutexes, notify that we're
  // complete (and commit any outstanding changes that should not be performed
  // while holding mutexes).
  DirectoryChangeEvent complete_event =
      { DirectoryChangeEvent::TRANSACTION_COMPLETE,
        NULL, NULL, INVALID };
  dirkernel_->changes_channel.Notify(complete_event);
}

ReadTransaction::ReadTransaction(Directory* directory, const char* file,
                                 int line)
  : BaseTransaction(directory, "Read", file, line, INVALID) {
}

ReadTransaction::ReadTransaction(const ScopedDirLookup& scoped_dir,
                                 const char* file, int line)
  : BaseTransaction(scoped_dir.operator -> (), "Read", file, line, INVALID) {
}

ReadTransaction::~ReadTransaction() {
  UnlockAndLog(NULL);
}

WriteTransaction::WriteTransaction(Directory* directory, WriterTag writer,
                                   const char* file, int line)
  : BaseTransaction(directory, "Write", file, line, writer),
    originals_(new OriginalEntries) {
}

WriteTransaction::WriteTransaction(const ScopedDirLookup& scoped_dir,
                                   WriterTag writer, const char* file, int line)
  : BaseTransaction(scoped_dir.operator -> (), "Write", file, line, writer),
    originals_(new OriginalEntries) {
}

WriteTransaction::WriteTransaction(Directory *directory)
    : BaseTransaction(directory),
      originals_(new OriginalEntries) {
}

void WriteTransaction::SaveOriginal(EntryKernel* entry) {
  if (NULL == entry)
    return;
  OriginalEntries::iterator i = originals_->lower_bound(*entry);
  if (i == originals_->end() ||
      i->ref(META_HANDLE) != entry->ref(META_HANDLE)) {
    originals_->insert(i, *entry);
  }
}

WriteTransaction::~WriteTransaction() {
  if (OFF != kInvariantCheckLevel) {
    const bool full_scan = (FULL_DB_VERIFICATION == kInvariantCheckLevel);
    if (full_scan)
      directory()->CheckTreeInvariants(this, full_scan);
    else
      directory()->CheckTreeInvariants(this, originals_);
  }

  UnlockAndLog(originals_);
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

const string& Entry::Get(StringField field) const {
  DCHECK(kernel_);
  return kernel_->ref(field);
}

syncable::ModelType Entry::GetServerModelType() const {
  ModelType specifics_type = GetModelTypeFromSpecifics(Get(SERVER_SPECIFICS));
  if (specifics_type != UNSPECIFIED)
    return specifics_type;
  if (IsRoot())
    return TOP_LEVEL_FOLDER;
  // Loose check for server-created top-level folders that aren't
  // bound to a particular model type.
  if (!Get(UNIQUE_SERVER_TAG).empty() && Get(SERVER_IS_DIR))
    return TOP_LEVEL_FOLDER;

  // Otherwise, we don't have a server type yet.  That should only happen
  // if the item is an uncommitted locally created item.
  // It's possible we'll need to relax these checks in the future; they're
  // just here for now as a safety measure.
  DCHECK(Get(IS_UNSYNCED));
  DCHECK(Get(SERVER_VERSION) == 0);
  DCHECK(Get(SERVER_IS_DEL));
  // Note: can't enforce !Get(ID).ServerKnows() here because that could
  // actually happen if we hit AttemptReuniteLostCommitResponses.
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
  kernel_ = new EntryKernel;
  ZeroFields(kernel_, BEGIN_FIELDS);
  kernel_->put(ID, trans->directory_->NextId());
  kernel_->put(META_HANDLE, trans->directory_->NextMetahandle());
  kernel_->mark_dirty(trans->directory_->kernel_->dirty_metahandles);
  kernel_->put(PARENT_ID, parent_id);
  kernel_->put(NON_UNIQUE_NAME, name);
  const int64 now = Now();
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
  kernel_ = new EntryKernel;
  ZeroFields(kernel_, BEGIN_FIELDS);
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
  if (is_del) {
    UnlinkFromOrder();
    dir()->Delete(kernel_);
    return true;
  } else {
    dir()->Undelete(kernel_);
    PutPredecessor(Id());  // Restores position to the 0th index.
    return true;
  }
}

bool MutableEntry::Put(Int64Field field, const int64& value) {
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
      PutPredecessor(Id());  // Fixes up the sibling order inconsistency.
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

MetahandleSet* MutableEntry::GetDirtyIndexHelper() {
  return dir()->kernel_->dirty_metahandles;
}

bool MutableEntry::PutUniqueClientTag(const string& new_tag) {
  // There is no SERVER_UNIQUE_CLIENT_TAG. This field is similar to ID.
  string old_tag = kernel_->ref(UNIQUE_CLIENT_TAG);
  if (old_tag == new_tag) {
    return true;
  }

  if (!new_tag.empty()) {
    // Make sure your new value is not in there already.
    EntryKernel lookup_kernel_ = *kernel_;
    lookup_kernel_.put(UNIQUE_CLIENT_TAG, new_tag);
    bool new_tag_conflicts =
        (dir()->kernel_->client_tag_index->count(&lookup_kernel_) > 0);
    if (new_tag_conflicts) {
      return false;
    }

    // We're sure that the new tag doesn't exist now so,
    // erase the old tag and finish up.
    dir()->kernel_->client_tag_index->erase(kernel_);
    kernel_->put(UNIQUE_CLIENT_TAG, new_tag);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
    CHECK(dir()->kernel_->client_tag_index->insert(kernel_).second);
  } else {
    // The new tag is empty. Since the old tag is not equal to the new tag,
    // The old tag isn't empty, and thus must exist in the index.
    CHECK(dir()->kernel_->client_tag_index->erase(kernel_));
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
      CHECK(1 == index->erase(kernel_->ref(META_HANDLE)));
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
    CHECK(predecessor.good());
    if (predecessor.Get(PARENT_ID) != Get(PARENT_ID))
      return false;
    successor_id = predecessor.Get(NEXT_ID);
    predecessor.Put(NEXT_ID, Get(ID));
  } else {
    syncable::Directory* dir = trans()->directory();
    successor_id = dir->GetFirstChildId(trans(), Get(PARENT_ID));
  }
  if (!successor_id.IsRoot()) {
    MutableEntry successor(write_transaction(), GET_BY_ID, successor_id);
    CHECK(successor.good());
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

Id Directory::GetChildWithNullIdField(IdField field,
                                      BaseTransaction* trans,
                                      const Id& parent_id) {
  // This query is O(number of children), which should be acceptable
  // when this method is used as the first step in enumerating the children of
  // a node.  But careless use otherwise could potentially result in
  // O((number of children)^2) performance.
  ChildHandles child_handles;
  GetChildHandles(trans, parent_id, &child_handles);
  ChildHandles::const_iterator it;
  for (it = child_handles.begin(); it != child_handles.end(); ++it) {
    Entry e(trans, GET_BY_HANDLE, *it);
    CHECK(e.good());
    if (e.Get(field).IsRoot())
      return e.Get(ID);
  }

  return Id();
}

Id Directory::GetFirstChildId(BaseTransaction* trans,
                              const Id& parent_id) {
  return GetChildWithNullIdField(PREV_ID, trans, parent_id);
}

Id Directory::GetLastChildId(BaseTransaction* trans,
                             const Id& parent_id) {
  return GetChildWithNullIdField(NEXT_ID, trans, parent_id);
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
       << EscapePath(
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

}  // namespace syncable
