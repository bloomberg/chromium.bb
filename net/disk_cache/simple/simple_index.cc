// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"

#if defined(OS_POSIX)
#include <sys/stat.h>
#include <sys/time.h>
#endif

namespace {

// How many seconds we delay writing the index to disk since the last cache
// operation has happened.
const int kWriteToDiskDelayMSecs = 20000;
const int kWriteToDiskOnBackgroundDelayMSecs = 100;

// Divides the cache space into this amount of parts to evict when only one part
// is left.
const uint32 kEvictionMarginDivisor = 20;

// Utility class used for timestamp comparisons in entry metadata while sorting.
class CompareHashesForTimestamp {
  typedef disk_cache::SimpleIndex SimpleIndex;
  typedef disk_cache::SimpleIndex::EntrySet EntrySet;
 public:
  explicit CompareHashesForTimestamp(const EntrySet& set);

  bool operator()(uint64 hash1, uint64 hash2);
 private:
  const EntrySet& entry_set_;
};

CompareHashesForTimestamp::CompareHashesForTimestamp(const EntrySet& set)
  : entry_set_(set) {
}

bool CompareHashesForTimestamp::operator()(uint64 hash1, uint64 hash2) {
  EntrySet::const_iterator it1 = entry_set_.find(hash1);
  DCHECK(it1 != entry_set_.end());
  EntrySet::const_iterator it2 = entry_set_.find(hash2);
  DCHECK(it2 != entry_set_.end());
  return it1->second.GetLastUsedTime() < it2->second.GetLastUsedTime();
}

bool GetNanoSecsFromStat(const struct stat& st, long* out_sec, long* out_nsec) {
#if defined(OS_ANDROID)
  *out_sec = st.st_mtime;
  *out_nsec = st.st_mtime_nsec;
#elif defined(OS_LINUX)
  *out_sec = st.st_mtim.tv_sec;
  *out_nsec = st.st_mtim.tv_nsec;
#elif defined(OS_MACOSX) || defined(OS_IOS) || defined(OS_BSD)
  *out_sec = st.st_mtimespec.tv_sec;
  *out_nsec = st.st_mtimespec.tv_nsec;
#else
  return false;
#endif
  return true;
}

bool GetMTime(const base::FilePath& path, base::Time* out_mtime) {
  DCHECK(out_mtime);
#if defined(OS_POSIX)
  base::ThreadRestrictions::AssertIOAllowed();
  struct stat file_stat;
  if (stat(path.value().c_str(), &file_stat) != 0)
    return false;
  long sec;
  long nsec;
  if (GetNanoSecsFromStat(file_stat, &sec, &nsec)) {
    int64 usec = (nsec / base::Time::kNanosecondsPerMicrosecond);
    *out_mtime = base::Time::FromTimeT(implicit_cast<time_t>(sec))
        + base::TimeDelta::FromMicroseconds(usec);
    return true;
  }
#endif
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(path, &file_info))
    return false;
  *out_mtime = file_info.last_modified;
  return true;
}

}  // namespace

namespace disk_cache {

EntryMetadata::EntryMetadata() : hash_key_(0),
                                 last_used_time_(0),
                                 entry_size_(0) {
}

EntryMetadata::EntryMetadata(uint64 hash_key,
                             base::Time last_used_time,
                             uint64 entry_size) :
    hash_key_(hash_key),
    last_used_time_(last_used_time.ToInternalValue()),
    entry_size_(entry_size) {
}

base::Time EntryMetadata::GetLastUsedTime() const {
  return base::Time::FromInternalValue(last_used_time_);
}

void EntryMetadata::SetLastUsedTime(const base::Time& last_used_time) {
  last_used_time_ = last_used_time.ToInternalValue();
}

void EntryMetadata::Serialize(Pickle* pickle) const {
  DCHECK(pickle);
  COMPILE_ASSERT(sizeof(EntryMetadata) ==
                 (sizeof(uint64) + sizeof(int64) + sizeof(uint64)),
                 EntryMetadata_has_three_member_variables);
  pickle->WriteUInt64(hash_key_);
  pickle->WriteInt64(last_used_time_);
  pickle->WriteUInt64(entry_size_);
}

bool EntryMetadata::Deserialize(PickleIterator* it) {
  DCHECK(it);
  return it->ReadUInt64(&hash_key_) &&
      it->ReadInt64(&last_used_time_) &&
      it->ReadUInt64(&entry_size_);
}

void EntryMetadata::MergeWith(const EntryMetadata& from) {
  DCHECK_EQ(hash_key_, from.hash_key_);
  if (last_used_time_ == 0)
    last_used_time_ = from.last_used_time_;
  if (entry_size_ == 0)
    entry_size_ = from.entry_size_;
}

SimpleIndex::SimpleIndex(base::SingleThreadTaskRunner* cache_thread,
                         base::SingleThreadTaskRunner* io_thread,
                         const base::FilePath& path)
    : cache_size_(0),
      max_size_(0),
      high_watermark_(0),
      low_watermark_(0),
      eviction_in_progress_(false),
      initialized_(false),
      index_filename_(path.AppendASCII("the-real-index")),
      cache_thread_(cache_thread),
      io_thread_(io_thread),
      app_on_background_(false) {
}

SimpleIndex::~SimpleIndex() {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Fail all callbacks waiting for the index to come up.
  for (CallbackList::iterator it = to_run_when_initialized_.begin(),
       end = to_run_when_initialized_.end(); it != end; ++it) {
    it->Run(net::ERR_ABORTED);
  }
}

void SimpleIndex::Initialize() {
  DCHECK(io_thread_checker_.CalledOnValidThread());

#if defined(OS_ANDROID)
  activity_status_listener_.reset(new base::android::ActivityStatus::Listener(
      base::Bind(&SimpleIndex::OnActivityStateChange, AsWeakPtr())));
#endif

  IndexCompletionCallback merge_callback =
      base::Bind(&SimpleIndex::MergeInitializingSet, AsWeakPtr());
  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&SimpleIndex::InitializeInternal,
                                        index_filename_,
                                        io_thread_,
                                        merge_callback),
                             true);
}

bool SimpleIndex::SetMaxSize(int max_bytes) {
  if (max_bytes < 0)
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  max_size_ = max_bytes;
  high_watermark_ = max_size_ - max_size_ / kEvictionMarginDivisor;
  low_watermark_ = max_size_ - 2 * (max_size_ / kEvictionMarginDivisor);
  return true;
}

int SimpleIndex::ExecuteWhenReady(const net::CompletionCallback& task) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (initialized_)
    io_thread_->PostTask(FROM_HERE, base::Bind(task, net::OK));
  else
    to_run_when_initialized_.push_back(task);
  return net::ERR_IO_PENDING;
}

scoped_ptr<std::vector<uint64> > SimpleIndex::RemoveEntriesBetween(
    const base::Time initial_time, const base::Time end_time) {
  DCHECK_EQ(true, initialized_);
  const base::Time extended_end_time =
      end_time.is_null() ? base::Time::Max() : end_time;
  DCHECK(extended_end_time >= initial_time);
  scoped_ptr<std::vector<uint64> > ret_hashes(new std::vector<uint64>());
  for (EntrySet::iterator it = entries_set_.begin(), end = entries_set_.end();
       it != end;) {
    EntryMetadata metadata = it->second;
    base::Time entry_time = metadata.GetLastUsedTime();
    if (initial_time <= entry_time && entry_time < extended_end_time) {
      ret_hashes->push_back(metadata.GetHashKey());
      entries_set_.erase(it++);
      cache_size_ -= metadata.GetEntrySize();
    } else {
      it++;
    }
  }
  return ret_hashes.Pass();
}

int32 SimpleIndex::GetEntryCount() const {
  // TODO(pasko): return a meaningful initial estimate before initialized.
  return entries_set_.size();
}

void SimpleIndex::Insert(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // Upon insert we don't know yet the size of the entry.
  // It will be updated later when the SimpleEntryImpl finishes opening or
  // creating the new entry, and then UpdateEntrySize will be called.
  const uint64 hash_key = simple_util::GetEntryHashKey(key);
  InsertInEntrySet(EntryMetadata(hash_key, base::Time::Now(), 0),
                   &entries_set_);
  if (!initialized_)
    removed_entries_.erase(hash_key);
  PostponeWritingToDisk();
}

void SimpleIndex::Remove(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  UpdateEntrySize(key, 0);
  const uint64 hash_key = simple_util::GetEntryHashKey(key);
  entries_set_.erase(hash_key);

  if (!initialized_)
    removed_entries_.insert(hash_key);
  PostponeWritingToDisk();
}

bool SimpleIndex::Has(const std::string& key) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // If not initialized, always return true, forcing it to go to the disk.
  return !initialized_ ||
      entries_set_.count(simple_util::GetEntryHashKey(key)) != 0;
}

bool SimpleIndex::UseIfExists(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // Always update the last used time, even if it is during initialization.
  // It will be merged later.
  EntrySet::iterator it = entries_set_.find(simple_util::GetEntryHashKey(key));
  if (it == entries_set_.end())
    // If not initialized, always return true, forcing it to go to the disk.
    return !initialized_;
  it->second.SetLastUsedTime(base::Time::Now());
  PostponeWritingToDisk();
  return true;
}

void SimpleIndex::StartEvictionIfNeeded() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (eviction_in_progress_ || cache_size_ <= high_watermark_)
    return;

  // Take all live key hashes from the index and sort them by time.
  eviction_in_progress_ = true;
  eviction_start_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_COUNTS("SimpleCache.Eviction.CacheSizeOnStart", cache_size_);
  UMA_HISTOGRAM_COUNTS("SimpleCache.Eviction.MaxCacheSizeOnStart", max_size_);
  scoped_ptr<std::vector<uint64> > entry_hashes(new std::vector<uint64>());
  for (EntrySet::const_iterator it = entries_set_.begin(),
       end = entries_set_.end(); it != end; ++it) {
    entry_hashes->push_back(it->second.GetHashKey());
  }
  std::sort(entry_hashes->begin(), entry_hashes->end(),
            CompareHashesForTimestamp(entries_set_));

  // Remove as many entries from the index to get below |low_watermark_|.
  std::vector<uint64>::iterator it = entry_hashes->begin();
  uint64 evicted_so_far_size = 0;
  while (evicted_so_far_size < cache_size_ - low_watermark_) {
    DCHECK(it != entry_hashes->end());
    EntrySet::iterator found_meta = entries_set_.find(*it);
    DCHECK(found_meta != entries_set_.end());
    uint64 to_evict_size = found_meta->second.GetEntrySize();
    evicted_so_far_size += to_evict_size;
    entries_set_.erase(found_meta);
    ++it;
  }
  cache_size_ -= evicted_so_far_size;

  // Take out the rest of hashes from the eviction list.
  entry_hashes->erase(it, entry_hashes->end());
  UMA_HISTOGRAM_COUNTS("SimpleCache.Eviction.EntryCount", entry_hashes->size());
  UMA_HISTOGRAM_TIMES("SimpleCache.Eviction.TimeToSelectEntries",
                      base::TimeTicks::Now() - eviction_start_time_);
  UMA_HISTOGRAM_COUNTS("SimpleCache.Eviction.SizeOfEvicted",
                       evicted_so_far_size);

  scoped_ptr<int> result(new int());
  base::Closure task = base::Bind(&SimpleSynchronousEntry::DoomEntrySet,
                                  base::Passed(&entry_hashes),
                                  index_filename_.DirName(), result.get());
  base::Closure reply = base::Bind(&SimpleIndex::EvictionDone, AsWeakPtr(),
                                   base::Passed(&result));
  base::WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
}

bool SimpleIndex::UpdateEntrySize(const std::string& key, uint64 entry_size) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  EntrySet::iterator it = entries_set_.find(simple_util::GetEntryHashKey(key));
  if (it == entries_set_.end())
    return false;

  // Update the total cache size with the new entry size.
  DCHECK(cache_size_ - it->second.GetEntrySize() <= cache_size_);
  cache_size_ -= it->second.GetEntrySize();
  cache_size_ += entry_size;
  it->second.SetEntrySize(entry_size);
  PostponeWritingToDisk();
  StartEvictionIfNeeded();
  return true;
}

void SimpleIndex::EvictionDone(scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(result);

  // Ignore the result of eviction. We did our best.
  eviction_in_progress_ = false;
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.Eviction.Result", *result == net::OK);
  UMA_HISTOGRAM_TIMES("SimpleCache.Eviction.TimeToDone",
                      base::TimeTicks::Now() - eviction_start_time_);
  UMA_HISTOGRAM_COUNTS("SimpleCache.Eviction.SizeWhenDone", cache_size_);
}

// static
void SimpleIndex::InsertInEntrySet(
    const disk_cache::EntryMetadata& entry_metadata,
    EntrySet* entry_set) {
  DCHECK(entry_set);
  entry_set->insert(
      std::make_pair(entry_metadata.GetHashKey(), entry_metadata));
}

void SimpleIndex::PostponeWritingToDisk() {
  if (!initialized_)
    return;
  const int delay = app_on_background_ ? kWriteToDiskOnBackgroundDelayMSecs
                                       : kWriteToDiskDelayMSecs;
  // If the timer is already active, Start() will just Reset it, postponing it.
  write_to_disk_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(delay),
      base::Bind(&SimpleIndex::WriteToDisk, AsWeakPtr()));
}

// static
bool SimpleIndex::IsIndexFileStale(const base::FilePath& index_filename) {
  base::Time index_mtime;
  base::Time dir_mtime;
  if (!GetMTime(index_filename.DirName(), &dir_mtime))
    return true;
  if (!GetMTime(index_filename, &index_mtime))
    return true;
  // Index file last_modified must be equal to the directory last_modified since
  // the last operation we do is ReplaceFile in the
  // SimpleIndexFile::WriteToDisk().
  // If not true, we need to restore the index.
  return index_mtime < dir_mtime;
}

// static
void SimpleIndex::InitializeInternal(
    const base::FilePath& index_filename,
    base::SingleThreadTaskRunner* io_thread,
    const IndexCompletionCallback& completion_callback) {
  // TODO(felipeg): probably could load a stale index and use it for something.
  scoped_ptr<EntrySet> index_file_entries;
  // Only load if the index is not stale.
  const bool index_stale = SimpleIndex::IsIndexFileStale(index_filename);
  if (!index_stale) {
    const base::TimeTicks start = base::TimeTicks::Now();
    index_file_entries = SimpleIndexFile::LoadFromDisk(index_filename);
    UMA_HISTOGRAM_TIMES("SimpleCache.IndexLoadTime",
                        (base::TimeTicks::Now() - start));
  }

  UMA_HISTOGRAM_BOOLEAN("SimpleCache.IndexStale", index_stale);

  bool force_index_flush = false;
  if (!index_file_entries) {
    const base::TimeTicks start = base::TimeTicks::Now();
    index_file_entries = SimpleIndex::RestoreFromDisk(index_filename);
    UMA_HISTOGRAM_TIMES("SimpleCache.IndexRestoreTime",
                        (base::TimeTicks::Now() - start));

    // When we restore from disk we write the merged index file to disk right
    // away, this might save us from having to restore again next time.
    force_index_flush = true;
  }
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.IndexCorrupt",
                        (!index_stale && force_index_flush));

  // 0 means Restored, 1 means Loaded from disk.
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.IndexInitializeMethod",
                        !force_index_flush);

  io_thread->PostTask(FROM_HERE,
                      base::Bind(completion_callback,
                                 base::Passed(&index_file_entries),
                                 force_index_flush));
}

// static
scoped_ptr<SimpleIndex::EntrySet> SimpleIndex::RestoreFromDisk(
    const base::FilePath& index_filename) {
  using file_util::FileEnumerator;
  LOG(INFO) << "Simple Cache Index is being restored from disk.";

  file_util::Delete(index_filename, /* recursive = */ false);
  scoped_ptr<EntrySet> index_file_entries(new EntrySet());

  // TODO(felipeg,gavinp): Fix this once we have a one-file per entry format.
  COMPILE_ASSERT(kSimpleEntryFileCount == 3,
                 file_pattern_must_match_file_count);

  const int kFileSuffixLenght = std::string("_0").size();
  const base::FilePath::StringType file_pattern = FILE_PATH_LITERAL("*_[0-2]");
  FileEnumerator enumerator(index_filename.DirName(),
                            false /* recursive */,
                            FileEnumerator::FILES,
                            file_pattern);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    const base::FilePath::StringType base_name = file_path.BaseName().value();
    // Converting to std::string is OK since we never use UTF8 wide chars in our
    // file names.
    const std::string hash_name(base_name.begin(), base_name.end());
    const std::string hash_key_string =
        hash_name.substr(0, hash_name.size() - kFileSuffixLenght);
    uint64 hash_key = 0;
    if (!simple_util::GetEntryHashKeyFromHexString(
            hash_key_string, &hash_key)) {
      LOG(WARNING) << "Invalid Entry Hash Key filename while restoring "
                   << "Simple Index from disk: " << hash_name;
      // TODO(felipeg): Should we delete the invalid file here ?
      continue;
    }

    FileEnumerator::FindInfo find_info = {};
    enumerator.GetFindInfo(&find_info);
    base::Time last_used_time;
#if defined(OS_POSIX)
    // For POSIX systems, a last access time is available. However, it's not
    // guaranteed to be more accurate than mtime. It is no worse though.
    last_used_time = base::Time::FromTimeT(find_info.stat.st_atime);
#endif
    if (last_used_time.is_null())
      last_used_time = FileEnumerator::GetLastModifiedTime(find_info);

    int64 file_size = FileEnumerator::GetFilesize(find_info);
    EntrySet::iterator it = index_file_entries->find(hash_key);
    if (it == index_file_entries->end()) {
      InsertInEntrySet(EntryMetadata(hash_key, last_used_time, file_size),
                       index_file_entries.get());
    } else {
      // Summing up the total size of the entry through all the *_[0-2] files
      it->second.SetEntrySize(it->second.GetEntrySize() + file_size);
    }
  }
  return index_file_entries.Pass();
}


// static
void SimpleIndex::WriteToDiskInternal(const base::FilePath& index_filename,
                                      scoped_ptr<Pickle> pickle,
                                      const base::TimeTicks& start_time) {
  SimpleIndexFile::WriteToDisk(index_filename, *pickle);
  UMA_HISTOGRAM_TIMES("SimpleCache.IndexWriteToDiskTime",
                      (base::TimeTicks::Now() - start_time));

}

void SimpleIndex::MergeInitializingSet(scoped_ptr<EntrySet> index_file_entries,
                                       bool force_index_flush) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(index_file_entries);
  // First, remove the entries that are in the |removed_entries_| from both
  // sets.
  for (base::hash_set<uint64>::const_iterator it =
           removed_entries_.begin(); it != removed_entries_.end(); ++it) {
    entries_set_.erase(*it);
    index_file_entries->erase(*it);
  }

  // Recalculate the cache size while merging the two sets.
  for (EntrySet::const_iterator it = index_file_entries->begin();
       it != index_file_entries->end(); ++it) {
    // If there is already an entry in the current entries_set_, we need to
    // merge the new data there with the data loaded in the initialization.
    EntrySet::iterator current_entry = entries_set_.find(it->first);
    if (current_entry != entries_set_.end()) {
      // When Merging, existing valid data in the |current_entry| will prevail.
      cache_size_ -= current_entry->second.GetEntrySize();
      current_entry->second.MergeWith(it->second);
      cache_size_ += current_entry->second.GetEntrySize();
    } else {
      InsertInEntrySet(it->second, &entries_set_);
      cache_size_ += it->second.GetEntrySize();
    }
  }
  initialized_ = true;
  removed_entries_.clear();

  // The actual IO is asynchronous, so calling WriteToDisk() shouldn't slow down
  // much the merge.
  if (force_index_flush)
    WriteToDisk();

  UMA_HISTOGRAM_CUSTOM_COUNTS("SimpleCache.IndexInitializationWaiters",
                              to_run_when_initialized_.size(), 0, 100, 20);
  // Run all callbacks waiting for the index to come up.
  for (CallbackList::iterator it = to_run_when_initialized_.begin(),
       end = to_run_when_initialized_.end(); it != end; ++it) {
    io_thread_->PostTask(FROM_HERE, base::Bind((*it), net::OK));
  }
  to_run_when_initialized_.clear();
}

#if defined(OS_ANDROID)
void SimpleIndex::OnActivityStateChange(
    base::android::ActivityState state) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // For more info about android activities, see:
  // developer.android.com/training/basics/activity-lifecycle/pausing.html
  // These values are defined in the file ActivityStatus.java
  if (state == base::android::ACTIVITY_STATE_RESUMED) {
    app_on_background_ = false;
  } else if (state == base::android::ACTIVITY_STATE_STOPPED) {
    app_on_background_ = true;
    WriteToDisk();
  }
}
#endif

void SimpleIndex::WriteToDisk() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return;
  UMA_HISTOGRAM_CUSTOM_COUNTS("SimpleCache.IndexNumEntriesOnWrite",
                              entries_set_.size(), 0, 100000, 50);
  const base::TimeTicks start = base::TimeTicks::Now();
  last_write_to_disk_ = base::Time::Now();
  SimpleIndexFile::IndexMetadata index_metadata(entries_set_.size(),
                                                cache_size_);
  scoped_ptr<Pickle> pickle = SimpleIndexFile::Serialize(index_metadata,
                                                         entries_set_);
  cache_thread_->PostTask(FROM_HERE, base::Bind(
      &SimpleIndex::WriteToDiskInternal,
      index_filename_,
      base::Passed(&pickle),
      start));
}

}  // namespace disk_cache
