// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include <functional>
#include <iomanip>
#include <iterator>
#include <set>
#include <string>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/perftimer.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"
#include "chrome/browser/sync/syncable/syncable_columns.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/compat_file.h"
#include "chrome/browser/sync/util/crypto_helpers.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/fast_dump.h"
#include "chrome/browser/sync/util/path_helpers.h"

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

// if sizeof(time_t) != sizeof(int32) we need to alter or expand the sqlite
// datatype.
COMPILE_ASSERT(sizeof(time_t) == sizeof(int32), time_t_is_not_int32);

using browser_sync::FastDump;
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
#elif (defined(OS_LINUX) || defined(OS_MACOSX))
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
int ComparePathNames16(void*, int a_bytes, const void* a, int b_bytes,
                       const void* b) {
#if defined(OS_WIN)
  DCHECK_EQ(0, a_bytes % 2);
  DCHECK_EQ(0, b_bytes % 2);
  int result = CompareString(LOCALE_INVARIANT, NORM_IGNORECASE,
    static_cast<const PathChar*>(a), a_bytes / 2,
    static_cast<const PathChar*>(b), b_bytes / 2);
  CHECK(0 != result) << "Error comparing strings: " << GetLastError();
  return result - 2;  // Convert to -1, 0, 1
#elif defined(OS_LINUX)
  int result = base::strncasecmp(reinterpret_cast<const char *>(a),
                                 reinterpret_cast<const char *>(b),
                                 std::min(a_bytes, b_bytes));
  if (result != 0) {
    return result;
  } else {
    return a_bytes > b_bytes ? 1 : b_bytes > a_bytes ? -1 : 0;
  }
#elif defined(OS_MACOSX)
  CFStringRef a_str;
  CFStringRef b_str;
  a_str = CFStringCreateWithBytes(NULL, reinterpret_cast<const UInt8*>(a),
                                  a_bytes, kCFStringEncodingUTF8, FALSE);
  b_str = CFStringCreateWithBytes(NULL, reinterpret_cast<const UInt8*>(b),
                                  b_bytes, kCFStringEncodingUTF8, FALSE);
  CFComparisonResult res;
  res = CFStringCompare(a_str, b_str, kCFCompareCaseInsensitive);
  CFRelease(a_str);
  CFRelease(b_str);
  return res;
#else
#error no ComparePathNames16() for your OS
#endif
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

// TODO(ncarter): Rename!
int ComparePathNames(const PathString& a, const PathString& b) {
  const size_t val_size = sizeof(PathString::value_type);
  return ComparePathNames16(NULL, a.size() * val_size, a.data(),
                                  b.size() * val_size, b.data());
}

class LessParentIdAndNames {
 public:
  bool operator() (const syncable::EntryKernel* a,
                   const syncable::EntryKernel* b) const {
    if (a->ref(PARENT_ID) != b->ref(PARENT_ID))
      return a->ref(PARENT_ID) < b->ref(PARENT_ID);
    return ComparePathNames(a->ref(NAME), b->ref(NAME)) < 0;
  }
};

bool LessPathNames::operator() (const PathString& a,
                                const PathString& b) const {
  return ComparePathNames(a, b) < 0;
}

// static
Name Name::FromEntryKernel(EntryKernel* kernel) {
  PathString& sync_name_ref = kernel->ref(UNSANITIZED_NAME).empty() ?
      kernel->ref(NAME) : kernel->ref(UNSANITIZED_NAME);
  return Name(kernel->ref(NAME), sync_name_ref, kernel->ref(NON_UNIQUE_NAME));
}

///////////////////////////////////////////////////////////////////////////
// Directory

static const DirectoryChangeEvent kShutdownChangesEvent =
    { DirectoryChangeEvent::SHUTDOWN, 0, 0 };

Directory::Kernel::Kernel(const PathString& db_path,
                          const PathString& name,
                          const KernelLoadInfo& info)
: db_path(db_path),
  refcount(1),
  name_(name),
  metahandles_index(new Directory::MetahandlesIndex),
  ids_index(new Directory::IdsIndex),
  parent_id_and_names_index(new Directory::ParentIdAndNamesIndex),
  extended_attributes(new ExtendedAttributes),
  unapplied_update_metahandles(new MetahandleSet),
  unsynced_metahandles(new MetahandleSet),
  channel(new Directory::Channel(syncable::DIRECTORY_DESTROYED)),
  changes_channel(new Directory::ChangesChannel(kShutdownChangesEvent)),
  last_sync_timestamp_(info.kernel_info.last_sync_timestamp),
  initial_sync_ended_(info.kernel_info.initial_sync_ended),
  store_birthday_(info.kernel_info.store_birthday),
  cache_guid_(info.cache_guid),
  next_metahandle(info.max_metahandle + 1),
  next_id(info.kernel_info.next_id) {
  info_status_ = Directory::KERNEL_SHARE_INFO_VALID;
}

inline void DeleteEntry(EntryKernel* kernel) {
  delete kernel;
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
  delete changes_channel;
  delete unsynced_metahandles;
  delete unapplied_update_metahandles;
  delete extended_attributes;
  delete parent_id_and_names_index;
  delete ids_index;
  for_each(metahandles_index->begin(), metahandles_index->end(), DeleteEntry);
  delete metahandles_index;
}

Directory::Directory() : kernel_(NULL), store_(NULL) {
}

Directory::~Directory() {
  Close();
}

BOOL PathNameMatch(const PathString& pathname, const PathString& pathspec) {
#if defined(OS_WIN)
  // Note that if we go Vista only this is easier:
  // http://msdn2.microsoft.com/en-us/library/ms628611.aspx

  // PathMatchSpec strips spaces from the start of pathspec, so we compare those
  // ourselves.
  const PathChar* pathname_ptr = pathname.c_str();
  const PathChar* pathspec_ptr = pathspec.c_str();

  while (*pathname_ptr == ' ' && *pathspec_ptr == ' ')
     ++pathname_ptr, ++pathspec_ptr;

  // If we have more inital spaces in the pathspec than in the pathname then the
  // result from PathMatchSpec will be erronous.
  if (*pathspec_ptr == ' ')
    return FALSE;

  // PathMatchSpec also gets "confused" when there are ';' characters in name or
  // in spec. So, if we match (f.i.) ";" with ";" PathMatchSpec will return
  // FALSE (which is wrong). Luckily for us, we can easily fix this by
  // substituting ';' with ':' which is illegal character in file name and
  // we're not going to see it there. With ':' in path name and spec
  // PathMatchSpec works fine.
  if ((NULL == wcschr(pathname_ptr, L';')) &&
      (NULL == wcschr(pathspec_ptr, L';'))) {
    // No ';' in file name and in spec. Just pass it as it is.
    return ::PathMatchSpec(pathname_ptr, pathspec_ptr);
  }

  // We need to subst ';' with ':' in both, name and spec.
  PathString name_subst(pathname_ptr);
  PathString spec_subst(pathspec_ptr);

  PathString::size_type index = name_subst.find(L';');
  while (PathString::npos != index) {
    name_subst[index] = L':';
    index = name_subst.find(L';', index + 1);
  }

  index = spec_subst.find(L';');
  while (PathString::npos != index) {
    spec_subst[index] = L':';
    index = spec_subst.find(L';', index + 1);
  }

  return ::PathMatchSpec(name_subst.c_str(), spec_subst.c_str());
#else
  return 0 == ComparePathNames(pathname, pathspec);
#endif
}

DirOpenResult Directory::Open(const PathString& file_path,
                              const PathString& name) {
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
      kernel_->parent_id_and_names_index->insert(entry);
    kernel_->ids_index->insert(entry);
    if (entry->ref(IS_UNSYNCED))
      kernel_->unsynced_metahandles->insert(entry->ref(META_HANDLE));
    if (entry->ref(IS_UNAPPLIED_UPDATE))
      kernel_->unapplied_update_metahandles->insert(entry->ref(META_HANDLE));
  }
}

DirectoryBackingStore* Directory::CreateBackingStore(
    const PathString& dir_name, const PathString& backing_filepath) {
  return new DirectoryBackingStore(dir_name, backing_filepath);
}

DirOpenResult Directory::OpenImpl(const PathString& file_path,
                                  const PathString& name) {
  DCHECK_EQ(static_cast<DirectoryBackingStore*>(NULL), store_);
  const PathString db_path = ::GetFullPath(file_path);
  store_ = CreateBackingStore(name, db_path);

  KernelLoadInfo info;
  // Temporary indicies before kernel_ initialized in case Load fails. We 0(1)
  // swap these later.
  MetahandlesIndex metas_bucket;
  ExtendedAttributes xattrs_bucket;
  DirOpenResult result = store_->Load(&metas_bucket, &xattrs_bucket, &info);
  if (OPENED != result)
    return result;

  kernel_ = new Kernel(db_path, name, info);
  kernel_->metahandles_index->swap(metas_bucket);
  kernel_->extended_attributes->swap(xattrs_bucket);
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
  // First look up in memory
  kernel_->needle.ref(ID) = id;
  IdsIndex::iterator id_found = kernel_->ids_index->find(&kernel_->needle);
  if (id_found != kernel_->ids_index->end()) {
    // Found it in memory.  Easy.
    return *id_found;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByTag(const PathString& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  // We don't currently keep a separate index for the tags.  Since tags
  // only exist for server created items that are the first items
  // to be created in a store, they should have small metahandles.
  // So, we just iterate over the items in sorted metahandle order,
  // looking for a match.
  MetahandlesIndex& set = *kernel_->metahandles_index;
  for (MetahandlesIndex::iterator i = set.begin(); i != set.end(); ++i) {
    if ((*i)->ref(SINGLETON_TAG) == tag) {
      return *i;
    }
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByHandle(const int64 metahandle) {
  ScopedKernelLock lock(this);
  return GetEntryByHandle(metahandle, &lock);
}

EntryKernel* Directory::GetEntryByHandle(const int64 metahandle,
                                         ScopedKernelLock* lock) {
  // Look up in memory
  kernel_->needle.ref(META_HANDLE) = metahandle;
  MetahandlesIndex::iterator found =
    kernel_->metahandles_index->find(&kernel_->needle);
  if (found != kernel_->metahandles_index->end()) {
    // Found it in memory.  Easy.
    return *found;
  }
  return NULL;
}

EntryKernel* Directory::GetChildWithName(const Id& parent_id,
                                         const PathString& name) {
  ScopedKernelLock lock(this);
  return GetChildWithName(parent_id, name, &lock);
}

// Will return child entry if the folder is opened,
// otherwise it will return NULL.
EntryKernel* Directory::GetChildWithName(const Id& parent_id,
                                         const PathString& name,
                                         ScopedKernelLock* const lock) {
  PathString dbname = name;
  EntryKernel* parent = GetEntryById(parent_id, lock);
  if (parent == NULL)
    return NULL;
  return GetChildWithNameImpl(parent_id, dbname, lock);
}

// Will return child entry even when the folder is not opened. This is used by
// syncer to apply update when folder is closed.
EntryKernel* Directory::GetChildWithDBName(const Id& parent_id,
                                           const PathString& name) {
  ScopedKernelLock lock(this);
  return GetChildWithNameImpl(parent_id, name, &lock);
}

EntryKernel* Directory::GetChildWithNameImpl(const Id& parent_id,
                                             const PathString& name,
                                             ScopedKernelLock* const lock) {
  // First look up in memory:
  kernel_->needle.ref(NAME) = name;
  kernel_->needle.ref(PARENT_ID) = parent_id;
  ParentIdAndNamesIndex::iterator found =
    kernel_->parent_id_and_names_index->find(&kernel_->needle);
  if (found != kernel_->parent_id_and_names_index->end()) {
    // Found it in memory.  Easy.
    return *found;
  }
  return NULL;
}

// An interface to specify the details of which children
// GetChildHandles() is looking for.
struct PathMatcher {
  explicit PathMatcher(const Id& parent_id) : parent_id_(parent_id) { }
  virtual ~PathMatcher() { }
  enum MatchType {
    NO_MATCH,
    MATCH,
    // Means we found the only entry we're looking for in
    // memory so we don't need to check the DB.
    EXACT_MATCH
  };
  virtual MatchType PathMatches(const PathString& path) = 0;
  typedef Directory::ParentIdAndNamesIndex Index;
  virtual Index::iterator lower_bound(Index* index) = 0;
  virtual Index::iterator upper_bound(Index* index) = 0;
  const Id parent_id_;
  EntryKernel needle_;
};

// Matches all children.
struct AllPathsMatcher : public PathMatcher {
  explicit AllPathsMatcher(const Id& parent_id) : PathMatcher(parent_id) {
  }
  virtual MatchType PathMatches(const PathString& path) {
    return MATCH;
  }
  virtual Index::iterator lower_bound(Index* index) {
    needle_.ref(PARENT_ID) = parent_id_;
    needle_.ref(NAME).clear();
    return index->lower_bound(&needle_);
  }

  virtual Index::iterator upper_bound(Index* index) {
    needle_.ref(PARENT_ID) = parent_id_;
    needle_.ref(NAME).clear();
    Index::iterator i = index->upper_bound(&needle_),
      end = index->end();
    while (i != end  && (*i)->ref(PARENT_ID) == parent_id_)
      ++i;
    return i;
  }
};

// Matches an exact filename only; no wildcards.
struct ExactPathMatcher : public PathMatcher {
  ExactPathMatcher(const PathString& pathspec, const Id& parent_id)
    : PathMatcher(parent_id), pathspec_(pathspec) {
  }
  virtual MatchType PathMatches(const PathString& path) {
    return 0 == ComparePathNames(path, pathspec_) ? EXACT_MATCH : NO_MATCH;
  }
  virtual Index::iterator lower_bound(Index* index) {
    needle_.ref(PARENT_ID) = parent_id_;
    needle_.ref(NAME) = pathspec_;
    return index->lower_bound(&needle_);
  }
  virtual Index::iterator upper_bound(Index* index) {
    needle_.ref(PARENT_ID) = parent_id_;
    needle_.ref(NAME) = pathspec_;
    return index->upper_bound(&needle_);
  }
  const PathString pathspec_;
};

// Matches a pathspec with wildcards.
struct PartialPathMatcher : public PathMatcher {
  PartialPathMatcher(const PathString& pathspec,
                     PathString::size_type wildcard, const Id& parent_id)
    : PathMatcher(parent_id), pathspec_(pathspec) {
    if (0 == wildcard)
      return;
    lesser_.assign(pathspec_.data(), wildcard);
    greater_.assign(pathspec_.data(), wildcard);
    // Increment the last letter of greater so we can then less than
    // compare to it.
    PathString::size_type i = greater_.size() - 1;
    do {
    if (greater_[i] == std::numeric_limits<PathString::value_type>::max()) {
        greater_.resize(i);  // Try the preceding character.
        if (0 == i--)
          break;
      } else {
        greater_[i] += 1;
      }
      // Yes, there are cases where incrementing a character
      // actually decreases its position in the sort.  Example: 9 -> :
    } while (ComparePathNames(lesser_, greater_) >= 0);
  }

  virtual MatchType PathMatches(const PathString& path) {
    return PathNameMatch(path, pathspec_) ? MATCH : NO_MATCH;
  }

  virtual Index::iterator lower_bound(Index* index) {
    needle_.ref(PARENT_ID) = parent_id_;
    needle_.ref(NAME) = lesser_;
    return index->lower_bound(&needle_);
  }
  virtual Index::iterator upper_bound(Index* index) {
    if (greater_.empty()) {
      needle_.ref(PARENT_ID) = parent_id_;
      needle_.ref(NAME).clear();
      Index::iterator i = index->upper_bound(&needle_),
        end = index->end();
      while (i != end && (*i)->ref(PARENT_ID) == parent_id_)
        ++i;
      return i;
    } else {
      needle_.ref(PARENT_ID) = parent_id_;
      needle_.ref(NAME) = greater_;
      return index->lower_bound(&needle_);
    }
  }

  const PathString pathspec_;
  PathString lesser_;
  PathString greater_;
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
    ParentIdAndNamesIndex* const index =
      kernel_->parent_id_and_names_index;
    typedef ParentIdAndNamesIndex::iterator iterator;
    for (iterator i = matcher->lower_bound(index),
           end = matcher->upper_bound(index); i != end; ++i) {
      // root's parent_id is NULL in the db but 0 in memory, so
      // have avoid listing the root as its own child.
      if ((*i)->ref(ID) == (*i)->ref(PARENT_ID))
        continue;
      PathMatcher::MatchType match = matcher->PathMatches((*i)->ref(NAME));
      if (PathMatcher::NO_MATCH == match)
        continue;
      result->push_back((*i)->ref(META_HANDLE));
      if (PathMatcher::EXACT_MATCH == match)
        return;
    }
  }
}

EntryKernel* Directory::GetRootEntry() {
  return GetEntryById(Id());
}

EntryKernel* Directory::GetEntryByPath(const PathString& path) {
  CHECK(kernel_);
  EntryKernel* result = GetRootEntry();
  CHECK(result) << "There should always be a root node.";
  for (PathSegmentIterator<PathString> i(path), end;
       i != end && NULL != result; ++i) {
    result = GetChildWithName(result->ref(ID), *i);
  }
  return result;
}

void ZeroFields(EntryKernel* entry, int first_field) {
  int i = first_field;
  // Note that bitset<> constructor sets all bits to zero, and strings
  // initialize to empty.
  for ( ; i < INT64_FIELDS_END; ++i)
    entry->ref(static_cast<Int64Field>(i)) = 0;
  for ( ; i < ID_FIELDS_END; ++i)
    entry->ref(static_cast<IdField>(i)).Clear();
  for ( ; i < BIT_FIELDS_END; ++i)
    entry->ref(static_cast<BitField>(i)) = false;
  if (i < BLOB_FIELDS_END)
    i = BLOB_FIELDS_END;
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
  if (!entry->ref(IS_DEL))
    CHECK(kernel_->parent_id_and_names_index->insert(entry).second) << error;
  CHECK(kernel_->ids_index->insert(entry).second) << error;
}

bool Directory::Undelete(EntryKernel* const entry) {
  DCHECK(entry->ref(IS_DEL));
  ScopedKernelLock lock(this);
  if (NULL != GetChildWithName(entry->ref(PARENT_ID), entry->ref(NAME), &lock))
    return false;  // Would have duplicated existing entry.
  entry->ref(IS_DEL) = false;
  entry->dirty[IS_DEL] = true;
  CHECK(kernel_->parent_id_and_names_index->insert(entry).second);
  return true;
}

bool Directory::Delete(EntryKernel* const entry) {
  DCHECK(!entry->ref(IS_DEL));
  entry->ref(IS_DEL) = true;
  entry->dirty[IS_DEL] = true;
  ScopedKernelLock lock(this);
  CHECK(1 == kernel_->parent_id_and_names_index->erase(entry));
  return true;
}

bool Directory::ReindexId(EntryKernel* const entry, const Id& new_id) {
  ScopedKernelLock lock(this);
  if (NULL != GetEntryById(new_id, &lock))
    return false;
  CHECK(1 == kernel_->ids_index->erase(entry));
  entry->ref(ID) = new_id;
  CHECK(kernel_->ids_index->insert(entry).second);
  return true;
}

bool Directory::ReindexParentIdAndName(EntryKernel* const entry,
                                       const Id& new_parent_id,
                                       const PathString& new_name) {
  ScopedKernelLock lock(this);
  PathString new_indexed_name = new_name;
  if (entry->ref(IS_DEL)) {
    entry->ref(PARENT_ID) = new_parent_id;
    entry->ref(NAME) = new_indexed_name;
    return true;
  }

  // check for a case changing rename
  if (entry->ref(PARENT_ID) == new_parent_id &&
    0 == ComparePathNames(entry->ref(NAME), new_indexed_name)) {
    entry->ref(NAME) = new_indexed_name;
  } else {
    if (NULL != GetChildWithName(new_parent_id, new_indexed_name, &lock))
      return false;
    CHECK(1 == kernel_->parent_id_and_names_index->erase(entry));
    entry->ref(PARENT_ID) = new_parent_id;
    entry->ref(NAME) = new_indexed_name;
    CHECK(kernel_->parent_id_and_names_index->insert(entry).second);
  }
  return true;
}

// static
bool Directory::SafeToPurgeFromMemory(const EntryKernel* const entry) {
  return entry->ref(IS_DEL) && !entry->dirty.any() && !entry->ref(SYNCING) &&
         !entry->ref(IS_UNAPPLIED_UPDATE) && !entry->ref(IS_UNSYNCED);
}

void Directory::TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot) {
  ReadTransaction trans(this, __FILE__, __LINE__);
  ScopedKernelLock lock(this);
  // Deep copy dirty entries from kernel_->metahandles_index into snapshot and
  // clear dirty flags.
  for (MetahandlesIndex::iterator i = kernel_->metahandles_index->begin();
       i != kernel_->metahandles_index->end(); ++i) {
    EntryKernel* entry = *i;
    if (!entry->dirty.any())
      continue;
    snapshot->dirty_metas.insert(snapshot->dirty_metas.end(), *entry);
    entry->dirty.reset();
    // TODO(timsteele): The previous *windows only* SaveChanges code path seems
    // to have a bug in that the IS_NEW bit is not rolled back if the entire DB
    // transaction is rolled back, due to the "recent" windows optimization of
    // using a ReadTransaction rather than a WriteTransaction in SaveChanges.
    // This bit is only used to decide whether we should sqlite INSERT or
    // UPDATE, and if we are INSERTing we make sure to dirty all the fields so
    // as to overwrite the database default values.  For now, this is rectified
    // by flipping the bit to false here (note that the snapshot will contain
    // the "original" value), and then resetting it on failure in
    // HandleSaveChangesFailure, where "failure" is defined as "the DB
    // "transaction was rolled back". This is safe because the only user of this
    // bit is in fact SaveChanges, which enforces mutually exclusive access by
    // way of save_changes_mutex_.  The TODO is to consider abolishing this bit
    // in favor of using a sqlite INSERT OR REPLACE, which could(would?) imply
    // that all bits need to be written rather than just the dirty ones in
    // the BindArg helper function.
    entry->ref(IS_NEW) = false;
  }

  // Do the same for extended attributes.
  for (ExtendedAttributes::iterator i = kernel_->extended_attributes->begin();
       i != kernel_->extended_attributes->end(); ++i) {
    if (!i->second.dirty)
      continue;
    snapshot->dirty_xattrs[i->first] = i->second;
    i->second.dirty = false;
  }

  // Fill kernel_info_status and kernel_info.
  PersistedKernelInfo& info = snapshot->kernel_info;
  info.initial_sync_ended = kernel_->initial_sync_ended_;
  info.last_sync_timestamp = kernel_->last_sync_timestamp_;
  // To avoid duplicates when the process crashes, we record the next_id to be
  // greater magnitude than could possibly be reached before the next save
  // changes.  In other words, it's effectively impossible for the user to
  // generate 65536 new bookmarks in 3 seconds.
  info.next_id = kernel_->next_id - 65536;
  info.store_birthday = kernel_->store_birthday_;
  snapshot->kernel_info_status = kernel_->info_status_;
  // This one we reset on failure.
  kernel_->info_status_ = KERNEL_SHARE_INFO_VALID;
}

bool Directory::SaveChanges() {
  bool success = false;
  DCHECK(store_);

  AutoLock scoped_lock(kernel_->save_changes_mutex);

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
  kernel_->flushed_metahandles_.Push(0);  // Begin flush marker
  // Now drop everything we can out of memory.
  for (OriginalEntries::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    kernel_->needle.ref(META_HANDLE) = i->ref(META_HANDLE);
    MetahandlesIndex::iterator found =
        kernel_->metahandles_index->find(&kernel_->needle);
    EntryKernel* entry = (found == kernel_->metahandles_index->end() ?
                          NULL : *found);
    if (entry && SafeToPurgeFromMemory(entry)) {
      // We now drop deleted metahandles that are up to date on both the client
      // and the server.
      size_t num_erased = 0;
      kernel_->flushed_metahandles_.Push(entry->ref(META_HANDLE));
      num_erased = kernel_->ids_index->erase(entry);
      DCHECK(1 == num_erased);
      num_erased = kernel_->metahandles_index->erase(entry);
      DCHECK(1 == num_erased);
      delete entry;
    }
  }

  ExtendedAttributes::const_iterator i = snapshot.dirty_xattrs.begin();
  while (i != snapshot.dirty_xattrs.end()) {
    ExtendedAttributeKey key(i->first.metahandle, i->first.key);
    ExtendedAttributes::iterator found =
        kernel_->extended_attributes->find(key);
    if (found == kernel_->extended_attributes->end() ||
        found->second.dirty || !i->second.is_deleted) {
      ++i;
    } else {
      kernel_->extended_attributes->erase(found);
    }
  }
}

void Directory::HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot) {
  ScopedKernelLock lock(this);
  kernel_->info_status_ = KERNEL_SHARE_INFO_DIRTY;

  // Because we cleared dirty bits on the real entries when taking the snapshot,
  // we should make sure the fact that the snapshot was not persisted gets
  // reflected in the entries.  Not doing this would mean if no other changes
  // occur to the same fields of the entries in dirty_metas some changes could
  // end up being lost, if they also failed to be committed to the server.
  // Setting the bits ensures that SaveChanges will at least try again later.
  for (OriginalEntries::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    kernel_->needle.ref(META_HANDLE) = i->ref(META_HANDLE);
    MetahandlesIndex::iterator found =
        kernel_->metahandles_index->find(&kernel_->needle);
    if (found != kernel_->metahandles_index->end()) {
      (*found)->dirty |= i->dirty;
      (*found)->ref(IS_NEW) = i->ref(IS_NEW);
    }
  }

  for (ExtendedAttributes::const_iterator i = snapshot.dirty_xattrs.begin();
       i != snapshot.dirty_xattrs.end(); ++i) {
    ExtendedAttributeKey key(i->first.metahandle, i->first.key);
    ExtendedAttributes::iterator found =
        kernel_->extended_attributes->find(key);
    if (found != kernel_->extended_attributes->end())
      found->second.dirty = true;
  }
}

int64 Directory::last_sync_timestamp() const {
  ScopedKernelLock lock(this);
  return kernel_->last_sync_timestamp_;
}

void Directory::set_last_sync_timestamp(int64 timestamp) {
  ScopedKernelLock lock(this);
  if (kernel_->last_sync_timestamp_ == timestamp)
    return;
  kernel_->last_sync_timestamp_ = timestamp;
  kernel_->info_status_ = KERNEL_SHARE_INFO_DIRTY;
}

bool Directory::initial_sync_ended() const {
  ScopedKernelLock lock(this);
  return kernel_->initial_sync_ended_;
}

void Directory::set_initial_sync_ended(bool x) {
  ScopedKernelLock lock(this);
  if (kernel_->initial_sync_ended_ == x)
    return;
  kernel_->initial_sync_ended_ = x;
  kernel_->info_status_ = KERNEL_SHARE_INFO_DIRTY;
}

string Directory::store_birthday() const {
  ScopedKernelLock lock(this);
  return kernel_->store_birthday_;
}

void Directory::set_store_birthday(string store_birthday) {
  ScopedKernelLock lock(this);
  if (kernel_->store_birthday_ == store_birthday)
    return;
  kernel_->store_birthday_ = store_birthday;
  kernel_->info_status_ = KERNEL_SHARE_INFO_DIRTY;
}

string Directory::cache_guid() const {
  // No need to lock since nothing ever writes to it after load.
  return kernel_->cache_guid_;
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

void Directory::GetAllExtendedAttributes(BaseTransaction* trans,
                                         int64 metahandle,
                                         std::set<ExtendedAttribute>* result) {
  AttributeKeySet keys;
  GetExtendedAttributesList(trans, metahandle, &keys);
  AttributeKeySet::iterator iter;
  for (iter = keys.begin(); iter != keys.end(); ++iter) {
      ExtendedAttributeKey key(metahandle, *iter);
      ExtendedAttribute extended_attribute(trans, GET_BY_HANDLE, key);
      CHECK(extended_attribute.good());
      result->insert(extended_attribute);
  }
}

void Directory::GetExtendedAttributesList(BaseTransaction* trans,
    int64 metahandle, AttributeKeySet* result) {
  ExtendedAttributes::iterator iter;
  for (iter = kernel_->extended_attributes->begin();
       iter != kernel_->extended_attributes->end(); ++iter) {
    if (iter->first.metahandle == metahandle) {
      if (!iter->second.is_deleted)
        result->insert(iter->first.key);
    }
  }
}

void Directory::DeleteAllExtendedAttributes(WriteTransaction* trans,
                                            int64 metahandle) {
  AttributeKeySet keys;
  GetExtendedAttributesList(trans, metahandle, &keys);
  AttributeKeySet::iterator iter;
  for (iter = keys.begin(); iter != keys.end(); ++iter) {
    ExtendedAttributeKey key(metahandle, *iter);
    MutableExtendedAttribute attribute(trans, GET_BY_HANDLE, key);
    // This flags the attribute for deletion during SaveChanges.  At that time
    // any deleted attributes are purged from disk and memory.
    attribute.delete_attribute();
  }
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
      CHECK(!e.Get(NAME).empty()) << e;
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
    if (CHANGES_VERSION == base_version || 0 == base_version) {
      if (e.Get(IS_UNAPPLIED_UPDATE)) {
        // Unapplied new item.
        CHECK(e.Get(IS_DEL)) << e;
        CHECK(id.ServerKnows()) << e;
      } else {
        // Uncommitted item.
        if (!e.Get(IS_DEL)) {
          CHECK(e.Get(IS_UNSYNCED)) << e;
        }
        CHECK(0 == server_version) << e;
        CHECK(!id.ServerKnows()) << e;
      }
    } else {
      CHECK(id.ServerKnows());
    }
    ++entries_done;
    int64 elapsed_ms = check_timer.Elapsed().InMilliseconds();
    if (elapsed_ms > max_ms) {
      LOG(INFO) << "Cutting Invariant check short after " << elapsed_ms << "ms."
        " Processed " << entries_done << "/" << handles.size() << " entries";
      return;
    }
  }
  // I did intend to add a check here to ensure no entries had been pulled into
  // memory by this function, but we can't guard against another ReadTransaction
  // pulling entries into RAM
}

///////////////////////////////////////////////////////////////////////////////
// ScopedKernelLocks

ScopedKernelLock::ScopedKernelLock(const Directory* dir)
  :  scoped_lock_(dir->kernel_->mutex), dir_(const_cast<Directory*>(dir)) {
}

///////////////////////////////////////////////////////////////////////////
// Transactions
#if defined LOG_ALL || !defined NDEBUG
static const bool kLoggingInfo = true;
#else
static const bool kLoggingInfo = false;
#endif

void BaseTransaction::Lock() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  dirkernel_->transaction_mutex.Acquire();

  time_acquired_ = base::TimeTicks::Now();
  const base::TimeDelta elapsed = time_acquired_ - start_time;
  if (kLoggingInfo && elapsed.InMilliseconds() > 200) {
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

void BaseTransaction::UnlockAndLog(OriginalEntries* originals_arg) {
  dirkernel_->transaction_mutex.AssertAcquired();

  scoped_ptr<OriginalEntries> originals(originals_arg);
  const base::TimeDelta elapsed = base::TimeTicks::Now() - time_acquired_;
  if (kLoggingInfo && elapsed.InMilliseconds() > 50) {
    logging::LogMessage(source_file_, line_, logging::LOG_INFO).stream()
        << name_ << " transaction completed in " << elapsed.InSecondsF()
        << " seconds.";
  }

  if (NULL == originals.get() || originals->empty()) {
    dirkernel_->transaction_mutex.Release();
    return;
  }

  AutoLock scoped_lock(dirkernel_->changes_channel_mutex);
  // Tell listeners to calculate changes while we still have the mutex.
  DirectoryChangeEvent event = { DirectoryChangeEvent::CALCULATE_CHANGES,
                                 originals.get(), this, writer_ };
  dirkernel_->changes_channel->NotifyListeners(event);

  dirkernel_->transaction_mutex.Release();

  DirectoryChangeEvent complete_event =
      { DirectoryChangeEvent::TRANSACTION_COMPLETE,
        NULL, NULL, INVALID };
  dirkernel_->changes_channel->NotifyListeners(complete_event);
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

Entry::Entry(BaseTransaction* trans, GetByTag, const PathString& tag)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByTag(tag);
}

Entry::Entry(BaseTransaction* trans, GetByHandle, int64 metahandle)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByHandle(metahandle);
}

Entry::Entry(BaseTransaction* trans, GetByPath, const PathString& path)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetEntryByPath(path);
}

Entry::Entry(BaseTransaction* trans, GetByParentIdAndName, const Id& parentid,
             const PathString& name)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetChildWithName(parentid, name);
}

Entry::Entry(BaseTransaction* trans, GetByParentIdAndDBName, const Id& parentid,
             const PathString& name)
    : basetrans_(trans) {
  kernel_ = trans->directory()->GetChildWithDBName(parentid, name);
}


Directory* Entry::dir() const {
  return basetrans_->directory();
}

PathString Entry::Get(StringField field) const {
  DCHECK(kernel_);
  return kernel_->ref(field);
}

void Entry::GetAllExtendedAttributes(BaseTransaction* trans,
    std::set<ExtendedAttribute> *result) {
  dir()->GetAllExtendedAttributes(trans, kernel_->ref(META_HANDLE), result);
}

void Entry::GetExtendedAttributesList(BaseTransaction* trans,
    AttributeKeySet* result) {
  dir()->GetExtendedAttributesList(trans, kernel_->ref(META_HANDLE), result);
}

void Entry::DeleteAllExtendedAttributes(WriteTransaction *trans) {
  dir()->DeleteAllExtendedAttributes(trans, kernel_->ref(META_HANDLE));
}

///////////////////////////////////////////////////////////////////////////
// MutableEntry

MutableEntry::MutableEntry(WriteTransaction* trans, Create,
                           const Id& parent_id, const PathString& name)
  : Entry(trans), write_transaction_(trans) {
  if (NULL != trans->directory()->GetChildWithName(parent_id, name)) {
    kernel_ = NULL;  // would have duplicated an existing entry.
    return;
  }
  Init(trans, parent_id, name);
}


void MutableEntry::Init(WriteTransaction* trans, const Id& parent_id,
                        const PathString& name) {
  kernel_ = new EntryKernel;
  ZeroFields(kernel_, BEGIN_FIELDS);
  kernel_->ref(ID) = trans->directory_->NextId();
  kernel_->dirty[ID] = true;
  kernel_->ref(META_HANDLE) = trans->directory_->NextMetahandle();
  kernel_->dirty[META_HANDLE] = true;
  kernel_->ref(PARENT_ID) = parent_id;
  kernel_->dirty[PARENT_ID] = true;
  kernel_->ref(NAME) = name;
  kernel_->dirty[NAME] = true;
  kernel_->ref(NON_UNIQUE_NAME) = name;
  kernel_->dirty[NON_UNIQUE_NAME] = true;
  kernel_->ref(IS_NEW) = true;
  const int64 now = Now();
  kernel_->ref(CTIME) = now;
  kernel_->dirty[CTIME] = true;
  kernel_->ref(MTIME) = now;
  kernel_->dirty[MTIME] = true;
  // We match the database defaults here
  kernel_->ref(BASE_VERSION) = CHANGES_VERSION;
  trans->directory()->InsertEntry(kernel_);
  // Because this entry is new, it was originally deleted.
  kernel_->ref(IS_DEL) = true;
  trans->SaveOriginal(kernel_);
  kernel_->ref(IS_DEL) = false;
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
  kernel_->ref(ID) = id;
  kernel_->dirty[ID] = true;
  kernel_->ref(META_HANDLE) = trans->directory_->NextMetahandle();
  kernel_->dirty[META_HANDLE] = true;
  kernel_->ref(IS_DEL) = true;
  kernel_->dirty[IS_DEL] = true;
  kernel_->ref(IS_NEW) = true;
  // We match the database defaults here
  kernel_->ref(BASE_VERSION) = CHANGES_VERSION;
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

MutableEntry::MutableEntry(WriteTransaction* trans, GetByPath,
                           const PathString& path)
  : Entry(trans, GET_BY_PATH, path), write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByParentIdAndName,
                           const Id& parentid, const PathString& name)
  : Entry(trans, GET_BY_PARENTID_AND_NAME, parentid, name),
    write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByParentIdAndDBName,
                           const Id& parentid, const PathString& name)
  : Entry(trans, GET_BY_PARENTID_AND_DBNAME, parentid, name),
    write_transaction_(trans) {
  trans->SaveOriginal(kernel_);
}

bool MutableEntry::PutIsDel(bool is_del) {
  DCHECK(kernel_);
  if (is_del == kernel_->ref(IS_DEL))
    return true;
  if (is_del) {
    UnlinkFromOrder();
    if (!dir()->Delete(kernel_))
      return false;
    return true;
  } else {
    if (!dir()->Undelete(kernel_))
      return false;
    PutPredecessor(Id());  // Restores position to the 0th index.
    return true;
  }
}

bool MutableEntry::Put(Int64Field field, const int64& value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->ref(field) = value;
    kernel_->dirty[static_cast<int>(field)] = true;
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
      if (!dir()->ReindexParentIdAndName(kernel_, value, kernel_->ref(NAME)))
        return false;
    } else {
      kernel_->ref(field) = value;
    }
    kernel_->dirty[static_cast<int>(field)] = true;
  }
  return true;
}

bool MutableEntry::Put(BaseVersion field, int64 value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->ref(field) = value;
    kernel_->dirty[static_cast<int>(field)] = true;
  }
  return true;
}

bool MutableEntry::Put(StringField field, const PathString& value) {
  return PutImpl(field, value);
}

bool MutableEntry::PutImpl(StringField field, const PathString& value) {
  DCHECK(kernel_);
  if (kernel_->ref(field) != value) {
    if (NAME == field) {
      if (!dir()->ReindexParentIdAndName(kernel_, kernel_->ref(PARENT_ID),
          value))
        return false;
    } else {
      kernel_->ref(field) = value;
    }
    kernel_->dirty[static_cast<int>(field)] = true;
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
    kernel_->ref(field) = value;
    kernel_->dirty[static_cast<int>(field)] = true;
  }
  return true;
}

// Avoids temporary collision in index when renaming a bookmark to another
// folder.
bool MutableEntry::PutParentIdAndName(const Id& parent_id,
                                      const Name& name) {
  DCHECK(kernel_);
  const bool parent_id_changes = parent_id != kernel_->ref(PARENT_ID);
  bool db_name_changes = name.db_value() != kernel_->ref(NAME);
  if (parent_id_changes || db_name_changes) {
    if (!dir()->ReindexParentIdAndName(kernel_, parent_id,
                                       name.db_value()))
      return false;
  }
  Put(UNSANITIZED_NAME, name.GetUnsanitizedName());
  Put(NON_UNIQUE_NAME, name.non_unique_value());
  if (db_name_changes)
    kernel_->dirty[NAME] = true;
  if (parent_id_changes) {
    kernel_->dirty[PARENT_ID] = true;
    PutPredecessor(Id());  // Put in the 0th position.
  }
  return true;
}

void MutableEntry::UnlinkFromOrder() {
  Id old_previous = Get(PREV_ID);
  Id old_next = Get(NEXT_ID);

  // Self-looping signifies that this item is not in the order.  If we were to
  // set these to 0, we could get into trouble because this node might look
  // like the first node in the ordering.
  Put(NEXT_ID, Get(ID));
  Put(PREV_ID, Get(ID));

  if (!old_previous.IsRoot()) {
    if (old_previous == old_next) {
      // Note previous == next doesn't imply previous == next == Get(ID). We
      // could have prev==next=="c-XX" and Get(ID)=="sX..." if an item was added
      // and deleted before receiving the server ID in the commit response.
      CHECK((old_next == Get(ID)) || !old_next.ServerKnows());
      return;  // Done if we were already self-looped (hence unlinked).
    }
    MutableEntry previous_entry(write_transaction(), GET_BY_ID, old_previous);
    CHECK(previous_entry.good());
    previous_entry.Put(NEXT_ID, old_next);
  }

  if (!old_next.IsRoot()) {
    MutableEntry next_entry(write_transaction(), GET_BY_ID, old_next);
    CHECK(next_entry.good());
    next_entry.Put(PREV_ID, old_previous);
  }
}

bool MutableEntry::PutPredecessor(const Id& predecessor_id) {
  // TODO(ncarter): Maybe there should be an independent HAS_POSITION bit?
  if (!Get(IS_BOOKMARK_OBJECT))
    return true;
  UnlinkFromOrder();

  if (Get(IS_DEL)) {
    DCHECK(predecessor_id.IsNull());
    return true;
  }

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
    result = (kernel_->next_id)--;
    kernel_->info_status_ = KERNEL_SHARE_INFO_DIRTY;
  }
  DCHECK_LT(result, 0);
  return Id::CreateFromClientString(Int64ToString(result));
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

ExtendedAttribute::ExtendedAttribute(BaseTransaction* trans, GetByHandle,
                                     const ExtendedAttributeKey& key) {
  Directory::Kernel* const kernel = trans->directory()->kernel_;
  ScopedKernelLock lock(trans->directory());
  Init(trans, kernel, &lock, key);
}

bool ExtendedAttribute::Init(BaseTransaction* trans,
                             Directory::Kernel* const kernel,
                             ScopedKernelLock* lock,
                             const ExtendedAttributeKey& key) {
  i_ = kernel->extended_attributes->find(key);
  good_ = kernel->extended_attributes->end() != i_;
  return good_;
}

MutableExtendedAttribute::MutableExtendedAttribute(
    WriteTransaction* trans, GetByHandle,
    const ExtendedAttributeKey& key) :
        ExtendedAttribute(trans, GET_BY_HANDLE, key) {
}

MutableExtendedAttribute::MutableExtendedAttribute(
    WriteTransaction* trans, Create, const ExtendedAttributeKey& key) {
  Directory::Kernel* const kernel = trans->directory()->kernel_;
  ScopedKernelLock lock(trans->directory());
  if (!Init(trans, kernel, &lock, key)) {
    ExtendedAttributeValue val;
    val.dirty = true;
    i_ = kernel->extended_attributes->insert(std::make_pair(key, val)).first;
    good_ = true;
  }
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

// returns -1 if s contains any non [0-9] characters
static int PathStringToInteger(PathString s) {
  PathString::const_iterator i = s.begin();
  for (; i != s.end(); ++i) {
    if (PathString::npos == PathString(PSTR("0123456789")).find(*i))
      return -1;
  }
  return
#if !PATHSTRING_IS_STD_STRING
  _wtoi
#else
  atoi
#endif
  (s.c_str());
}

static PathString IntegerToPathString(int i) {
  const size_t kBufSize = 25;
  PathChar buf[kBufSize];
#if !PATHSTRING_IS_STD_STRING
  const int radix = 10;
  _itow(i, buf, radix);
#else
  snprintf(buf, kBufSize, "%d", i);
#endif
  return buf;
}

// appends ~1 to the end of 's' unless there is already ~#, in which case
// it just increments the number
static PathString FixBasenameInCollision(const PathString s) {
  PathString::size_type last_tilde = s.find_last_of(PSTR('~'));
  if (PathString::npos == last_tilde) return s + PSTR("~1");
  if (s.size() == (last_tilde + 1)) return s + PSTR("1");
  // we have ~, but not necessarily ~# (for some number >= 0). check for that
  int n;
  if ((n = PathStringToInteger(s.substr(last_tilde + 1))) != -1) {
    n++;
    PathString pre_number = s.substr(0, last_tilde + 1);
    return pre_number + IntegerToPathString(n);
  } else {
    // we have a ~, but not a number following it, so we'll add another
    // ~ and this time, a number
    return s + PSTR("~1");
  }
}

void DBName::MakeNoncollidingForEntry(BaseTransaction* trans,
                                      const Id& parent_id,
                                      Entry* e) {
  const PathString& desired_name = *this;
  CHECK(!desired_name.empty());
  PathString::size_type first_dot = desired_name.find_first_of(PSTR('.'));
  if (PathString::npos == first_dot)
    first_dot = desired_name.size();
  PathString basename = desired_name.substr(0, first_dot);
  PathString dotextension = desired_name.substr(first_dot);
  CHECK(basename + dotextension == desired_name);
  for (;;) {
    // Check for collision.
    PathString testname = basename + dotextension;
    Entry same_path_entry(trans, GET_BY_PARENTID_AND_DBNAME,
                          parent_id, testname);
    if (!same_path_entry.good() || (e && same_path_entry.Get(ID) == e->Get(ID)))
      break;
    // There was a collision, so fix the name.
    basename = FixBasenameInCollision(basename);
  }
  // Set our value to the new value.  This invalidates desired_name.
  PathString new_value = basename + dotextension;
  swap(new_value);
}

PathString GetFullPath(BaseTransaction* trans, const Entry& e) {
  PathString result;
#if defined(COMPILER_MSVC)
  result.reserve(MAX_PATH);
#endif
  ReverseAppend(e.Get(NAME), &result);
  Id id = e.Get(PARENT_ID);
  while (!id.IsRoot()) {
    result.push_back(kPathSeparator[0]);
    Entry ancestor(trans, GET_BY_ID, id);
    if (!ancestor.good()) {
      // This can happen if the parent folder got deleted before the entry.
      LOG(WARNING) << "Cannot get full path of " << e
                   << "\nbecause an ancestor folder has been deleted.";
      result.clear();
      return result;
    }
    ReverseAppend(ancestor.Get(NAME), &result);
    id = ancestor.Get(PARENT_ID);
  }
  result.push_back(kPathSeparator[0]);
  reverse(result.begin(), result.end());
  return result;
}

const Blob* GetExtendedAttributeValue(const Entry& e,
                                      const PathString& attribute_name) {
  ExtendedAttributeKey key(e.Get(META_HANDLE), attribute_name);
  ExtendedAttribute extended_attribute(e.trans(), GET_BY_HANDLE, key);
  if (extended_attribute.good() && !extended_attribute.is_deleted())
    return &extended_attribute.value();
  return NULL;
}

// This function sets only the flags needed to get this entry to sync.
void MarkForSyncing(syncable::MutableEntry* e) {
  DCHECK_NE(static_cast<MutableEntry*>(NULL), e);
  DCHECK(!e->IsRoot()) << "We shouldn't mark a permanent object for syncing.";
  e->Put(IS_UNSYNCED, true);
  e->Put(SYNCING, false);
}

}  // namespace syncable

namespace {
  class DumpSeparator {
  } separator;
  class DumpColon {
  } colon;
}  // namespace

inline FastDump& operator<<(FastDump& dump, const DumpSeparator&) {
  dump.out_->sputn(", ", 2);
  return dump;
}

inline FastDump& operator<<(FastDump& dump, const DumpColon&) {
  dump.out_->sputn(": ", 2);
  return dump;
}

std::ostream& operator<<(std::ostream& stream, const syncable::Entry& entry) {
  // Using ostreams directly here is dreadfully slow, because a mutex is
  // acquired for every <<.  Users noticed it spiking CPU.
  using browser_sync::ToUTF8;
  using syncable::BitField;
  using syncable::BitTemp;
  using syncable::BlobField;
  using syncable::EntryKernel;
  using syncable::g_metas_columns;
  using syncable::IdField;
  using syncable::Int64Field;
  using syncable::StringField;
  using syncable::BEGIN_FIELDS;
  using syncable::BIT_FIELDS_END;
  using syncable::BIT_TEMPS_BEGIN;
  using syncable::BIT_TEMPS_END;
  using syncable::BLOB_FIELDS_END;
  using syncable::INT64_FIELDS_END;
  using syncable::ID_FIELDS_END;
  using syncable::STRING_FIELDS_END;

  int i;
  FastDump s(&stream);
  EntryKernel* const kernel = entry.kernel_;
  for (i = BEGIN_FIELDS; i < INT64_FIELDS_END; ++i) {
    s << g_metas_columns[i].name << colon
      << kernel->ref(static_cast<Int64Field>(i)) << separator;
  }
  for ( ; i < ID_FIELDS_END; ++i) {
    s << g_metas_columns[i].name << colon
      << kernel->ref(static_cast<IdField>(i)) << separator;
  }
  s << "Flags: ";
  for ( ; i < BIT_FIELDS_END; ++i) {
    if (kernel->ref(static_cast<BitField>(i)))
      s << g_metas_columns[i].name << separator;
  }
  for ( ; i < STRING_FIELDS_END; ++i) {
    ToUTF8 field(kernel->ref(static_cast<StringField>(i)));
    s << g_metas_columns[i].name << colon << field.get_string() << separator;
  }
  for ( ; i < BLOB_FIELDS_END; ++i) {
    s << g_metas_columns[i].name << colon
      << kernel->ref(static_cast<BlobField>(i)) << separator;
  }
  s << "TempFlags: ";
  for ( ; i < BIT_TEMPS_END; ++i) {
    if (kernel->ref(static_cast<BitTemp>(i)))
      s << "#" << i - BIT_TEMPS_BEGIN << separator;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& s, const syncable::Blob& blob) {
  for (syncable::Blob::const_iterator i = blob.begin(); i != blob.end(); ++i)
    s << std::hex << std::setw(2)
      << std::setfill('0') << static_cast<unsigned int>(*i);
  return s << std::dec;
}

FastDump& operator<<(FastDump& dump, const syncable::Blob& blob) {
  if (blob.empty())
    return dump;
  string buffer(HexEncode(&blob[0], blob.size()));
  dump.out_->sputn(buffer.c_str(), buffer.size());
  return dump;
}
