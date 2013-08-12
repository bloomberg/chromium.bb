// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_index.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/task_runner.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
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

// How many milliseconds we delay writing the index to disk since the last cache
// operation has happened.
const int kDefaultWriteToDiskDelayMSecs = 20000;
const int kDefaultWriteToDiskOnBackgroundDelayMSecs = 100;

// Divides the cache space into this amount of parts to evict when only one part
// is left.
const uint32 kEvictionMarginDivisor = 20;

const uint32 kBytesInKb = 1024;

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

}  // namespace

namespace disk_cache {

EntryMetadata::EntryMetadata() : last_used_time_(0), entry_size_(0) {}

EntryMetadata::EntryMetadata(base::Time last_used_time, uint64 entry_size)
    : last_used_time_(last_used_time.ToInternalValue()),
      entry_size_(entry_size) {}

base::Time EntryMetadata::GetLastUsedTime() const {
  return base::Time::FromInternalValue(last_used_time_);
}

void EntryMetadata::SetLastUsedTime(const base::Time& last_used_time) {
  last_used_time_ = last_used_time.ToInternalValue();
}

void EntryMetadata::Serialize(Pickle* pickle) const {
  DCHECK(pickle);
  COMPILE_ASSERT(sizeof(EntryMetadata) == (sizeof(int64) + sizeof(uint64)),
                 EntryMetadata_has_two_member_variables);
  pickle->WriteInt64(last_used_time_);
  pickle->WriteUInt64(entry_size_);
}

bool EntryMetadata::Deserialize(PickleIterator* it) {
  DCHECK(it);
  return it->ReadInt64(&last_used_time_) && it->ReadUInt64(&entry_size_);
}

SimpleIndex::SimpleIndex(base::SingleThreadTaskRunner* io_thread,
                         const base::FilePath& cache_directory,
                         scoped_ptr<SimpleIndexFile> index_file)
    : cache_size_(0),
      max_size_(0),
      high_watermark_(0),
      low_watermark_(0),
      eviction_in_progress_(false),
      initialized_(false),
      cache_directory_(cache_directory),
      index_file_(index_file.Pass()),
      io_thread_(io_thread),
      // Creating the callback once so it is reused every time
      // write_to_disk_timer_.Start() is called.
      write_to_disk_cb_(base::Bind(&SimpleIndex::WriteToDisk, AsWeakPtr())),
      app_on_background_(false) {}

SimpleIndex::~SimpleIndex() {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Fail all callbacks waiting for the index to come up.
  for (CallbackList::iterator it = to_run_when_initialized_.begin(),
       end = to_run_when_initialized_.end(); it != end; ++it) {
    it->Run(net::ERR_ABORTED);
  }
}

void SimpleIndex::Initialize(base::Time cache_mtime) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Take the foreground and background index flush delays from the experiment
  // settings only if both are valid.
  foreground_flush_delay_ = kDefaultWriteToDiskDelayMSecs;
  background_flush_delay_ = kDefaultWriteToDiskOnBackgroundDelayMSecs;
  const std::string index_flush_intervals = base::FieldTrialList::FindFullName(
      "SimpleCacheIndexFlushDelay_Foreground_Background");
  if (!index_flush_intervals.empty()) {
    base::StringTokenizer tokens(index_flush_intervals, "_");
    int foreground_delay, background_delay;
    if (tokens.GetNext() &&
        base::StringToInt(tokens.token(), &foreground_delay) &&
        tokens.GetNext() &&
        base::StringToInt(tokens.token(), &background_delay)) {
      foreground_flush_delay_ = foreground_delay;
      background_flush_delay_ = background_delay;
    }
  }

#if defined(OS_ANDROID)
  activity_status_listener_.reset(new base::android::ActivityStatus::Listener(
      base::Bind(&SimpleIndex::OnActivityStateChange, AsWeakPtr())));
#endif

  SimpleIndexLoadResult* load_result = new SimpleIndexLoadResult();
  scoped_ptr<SimpleIndexLoadResult> load_result_scoped(load_result);
  base::Closure reply = base::Bind(
      &SimpleIndex::MergeInitializingSet,
      AsWeakPtr(),
      base::Passed(&load_result_scoped));
  index_file_->LoadIndexEntries(cache_mtime, reply, load_result);
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

scoped_ptr<SimpleIndex::HashList> SimpleIndex::RemoveEntriesBetween(
    const base::Time initial_time, const base::Time end_time) {
  return ExtractEntriesBetween(initial_time, end_time, true);
}

scoped_ptr<SimpleIndex::HashList> SimpleIndex::GetAllHashes() {
  const base::Time null_time = base::Time();
  return ExtractEntriesBetween(null_time, null_time, false);
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
  InsertInEntrySet(
      hash_key, EntryMetadata(base::Time::Now(), 0), &entries_set_);
  if (!initialized_)
    removed_entries_.erase(hash_key);
  PostponeWritingToDisk();
}

void SimpleIndex::Remove(const std::string& key) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  const uint64 hash_key = simple_util::GetEntryHashKey(key);
  EntrySet::iterator it = entries_set_.find(hash_key);
  if (it != entries_set_.end()) {
    UpdateEntryIteratorSize(&it, 0);
    entries_set_.erase(it);
  }

  if (!initialized_)
    removed_entries_.insert(hash_key);
  PostponeWritingToDisk();
}

bool SimpleIndex::Has(uint64 hash) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // If not initialized, always return true, forcing it to go to the disk.
  return !initialized_ || entries_set_.count(hash) > 0;
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
  UMA_HISTOGRAM_MEMORY_KB("SimpleCache.Eviction.CacheSizeOnStart2",
                          cache_size_ / kBytesInKb);
  UMA_HISTOGRAM_MEMORY_KB("SimpleCache.Eviction.MaxCacheSizeOnStart2",
                          max_size_ / kBytesInKb);
  scoped_ptr<std::vector<uint64> > entry_hashes(new std::vector<uint64>());
  for (EntrySet::const_iterator it = entries_set_.begin(),
       end = entries_set_.end(); it != end; ++it) {
    entry_hashes->push_back(it->first);
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
  UMA_HISTOGRAM_MEMORY_KB("SimpleCache.Eviction.SizeOfEvicted2",
                          evicted_so_far_size / kBytesInKb);

  index_file_->DoomEntrySet(
      entry_hashes.Pass(),
      base::Bind(&SimpleIndex::EvictionDone, AsWeakPtr()));
}

bool SimpleIndex::UpdateEntrySize(const std::string& key, uint64 entry_size) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  EntrySet::iterator it = entries_set_.find(simple_util::GetEntryHashKey(key));
  if (it == entries_set_.end())
    return false;

  UpdateEntryIteratorSize(&it, entry_size);
  PostponeWritingToDisk();
  StartEvictionIfNeeded();
  return true;
}

void SimpleIndex::EvictionDone(int result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());

  // Ignore the result of eviction. We did our best.
  eviction_in_progress_ = false;
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.Eviction.Result", result == net::OK);
  UMA_HISTOGRAM_TIMES("SimpleCache.Eviction.TimeToDone",
                      base::TimeTicks::Now() - eviction_start_time_);
  UMA_HISTOGRAM_MEMORY_KB("SimpleCache.Eviction.SizeWhenDone2",
                          cache_size_ / kBytesInKb);
}

// static
void SimpleIndex::InsertInEntrySet(
    uint64 hash_key,
    const disk_cache::EntryMetadata& entry_metadata,
    EntrySet* entry_set) {
  DCHECK(entry_set);
  entry_set->insert(std::make_pair(hash_key, entry_metadata));
}

void SimpleIndex::PostponeWritingToDisk() {
  if (!initialized_)
    return;
  const int delay = app_on_background_ ? background_flush_delay_
                                       : foreground_flush_delay_;
  // If the timer is already active, Start() will just Reset it, postponing it.
  write_to_disk_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(delay), write_to_disk_cb_);
}

void SimpleIndex::UpdateEntryIteratorSize(EntrySet::iterator* it,
                                          uint64 entry_size) {
  // Update the total cache size with the new entry size.
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_GE(cache_size_, (*it)->second.GetEntrySize());
  cache_size_ -= (*it)->second.GetEntrySize();
  cache_size_ += entry_size;
  (*it)->second.SetEntrySize(entry_size);
}

void SimpleIndex::MergeInitializingSet(
    scoped_ptr<SimpleIndexLoadResult> load_result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(load_result->did_load);

  SimpleIndex::EntrySet* index_file_entries = &load_result->entries;
  // First, remove the entries that are in the |removed_entries_| from both
  // sets.
  for (base::hash_set<uint64>::const_iterator it =
           removed_entries_.begin(); it != removed_entries_.end(); ++it) {
    entries_set_.erase(*it);
    index_file_entries->erase(*it);
  }

  for (EntrySet::const_iterator it = entries_set_.begin();
       it != entries_set_.end(); ++it) {
    const uint64 entry_hash = it->first;
    std::pair<EntrySet::iterator, bool> insert_result =
        index_file_entries->insert(EntrySet::value_type(entry_hash,
                                                        EntryMetadata()));
    EntrySet::iterator& possibly_inserted_entry = insert_result.first;
    possibly_inserted_entry->second = it->second;
  }

  uint64 merged_cache_size = 0;
  for (EntrySet::iterator it = index_file_entries->begin();
       it != index_file_entries->end(); ++it) {
    merged_cache_size += it->second.GetEntrySize();
  }

  entries_set_.swap(*index_file_entries);
  cache_size_ = merged_cache_size;
  initialized_ = true;
  removed_entries_.clear();

  // The actual IO is asynchronous, so calling WriteToDisk() shouldn't slow the
  // merge down much.
  if (load_result->flush_required)
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
  if (!last_write_to_disk_.is_null()) {
    if (app_on_background_) {
      UMA_HISTOGRAM_MEDIUM_TIMES("SimpleCache.IndexWriteInterval.Background",
                                 start - last_write_to_disk_);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES("SimpleCache.IndexWriteInterval.Foreground",
                                 start - last_write_to_disk_);
    }
  }
  last_write_to_disk_ = start;

  index_file_->WriteToDisk(entries_set_, cache_size_,
                           start, app_on_background_);
}

scoped_ptr<SimpleIndex::HashList> SimpleIndex::ExtractEntriesBetween(
    const base::Time initial_time, const base::Time end_time,
    bool delete_entries) {
  DCHECK_EQ(true, initialized_);
  const base::Time extended_end_time =
      end_time.is_null() ? base::Time::Max() : end_time;
  DCHECK(extended_end_time >= initial_time);
  scoped_ptr<HashList> ret_hashes(new HashList());
  for (EntrySet::iterator it = entries_set_.begin(), end = entries_set_.end();
       it != end;) {
    EntryMetadata& metadata = it->second;
    base::Time entry_time = metadata.GetLastUsedTime();
    if (initial_time <= entry_time && entry_time < extended_end_time) {
      ret_hashes->push_back(it->first);
      if (delete_entries) {
        cache_size_ -= metadata.GetEntrySize();
        entries_set_.erase(it++);
        continue;
      }
    }
    ++it;
  }
  return ret_hashes.Pass();
}

}  // namespace disk_cache
