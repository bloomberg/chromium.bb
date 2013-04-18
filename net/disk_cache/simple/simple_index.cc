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
#include "base/pickle.h"
#include "base/task_runner.h"
#include "base/threading/worker_pool.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_util.h"

namespace {

// How many seconds we delay writing the index to disk since the last cache
// operation has happened.
const int kWriteToDiskDelaySecs = 20;

// WriteToDisk at lest every 5 minutes.
const int kMaxWriteToDiskDelaySecs = 300;

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

SimpleIndex::SimpleIndex(
    base::SingleThreadTaskRunner* cache_thread,
    base::SingleThreadTaskRunner* io_thread,
    const base::FilePath& path)
    : cache_size_(0),
      initialized_(false),
      index_filename_(path.AppendASCII("simple-index")),
      cache_thread_(cache_thread),
      io_thread_(io_thread) {
}

SimpleIndex::~SimpleIndex() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
}

void SimpleIndex::Initialize() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  IndexCompletionCallback merge_callback =
      base::Bind(&SimpleIndex::MergeInitializingSet, AsWeakPtr());
  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&SimpleIndex::LoadFromDisk,
                                        index_filename_,
                                        io_thread_,
                                        merge_callback),
                             true);
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

bool SimpleIndex::UpdateEntrySize(const std::string& key, uint64 entry_size) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  EntrySet::iterator it = entries_set_.find(simple_util::GetEntryHashKey(key));
  if (it == entries_set_.end())
    return false;

  // Update the total cache size with the new entry size.
  cache_size_ -= it->second.GetEntrySize();
  cache_size_ += entry_size;
  it->second.SetEntrySize(entry_size);
  PostponeWritingToDisk();
  return true;
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
  const base::TimeDelta file_age = base::Time::Now() - last_write_to_disk_;
  if (file_age > base::TimeDelta::FromSeconds(kMaxWriteToDiskDelaySecs) &&
      write_to_disk_timer_.IsRunning()) {
    // If the index file is too old and there is a timer programmed to run a
    // WriteToDisk soon, we don't postpone it, so we always WriteToDisk
    // approximately every kMaxWriteToDiskDelaySecs.
    return;
  }

  // If the timer is already active, Start() will just Reset it, postponing it.
  write_to_disk_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kWriteToDiskDelaySecs),
      base::Bind(&SimpleIndex::WriteToDisk, AsWeakPtr()));
}

// static
void SimpleIndex::LoadFromDisk(
    const base::FilePath& index_filename,
    base::SingleThreadTaskRunner* io_thread,
    const IndexCompletionCallback& completion_callback) {
  scoped_ptr<EntrySet> index_file_entries =
      SimpleIndexFile::LoadFromDisk(index_filename);

  bool force_index_flush = false;
  if (!index_file_entries.get()) {
    index_file_entries = SimpleIndex::RestoreFromDisk(index_filename);
    // When we restore from disk we write the merged index file to disk right
    // away, this might save us from having to restore again next time.
    force_index_flush = true;
  }

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
                                      scoped_ptr<Pickle> pickle) {
  SimpleIndexFile::WriteToDisk(index_filename, *pickle);
}

void SimpleIndex::MergeInitializingSet(scoped_ptr<EntrySet> index_file_entries,
                                       bool force_index_flush) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // First, remove the entries that are in the |removed_entries_| from both
  // sets.
  for (base::hash_set<uint64>::const_iterator it =
           removed_entries_.begin(); it != removed_entries_.end(); ++it) {
    entries_set_.erase(*it);
    index_file_entries->erase(*it);
  }

  // Recalculate the cache size while merging the two sets.
  cache_size_ = 0;
  for (EntrySet::const_iterator it = index_file_entries->begin();
       it != index_file_entries->end(); ++it) {
    // If there is already an entry in the current entries_set_, we need to
    // merge the new data there with the data loaded in the initialization.
    EntrySet::iterator current_entry = entries_set_.find(it->first);
    if (current_entry != entries_set_.end()) {
      // When Merging, existing valid data in the |current_entry| will prevail.
      current_entry->second.MergeWith(it->second);
      cache_size_ += current_entry->second.GetEntrySize();
    } else {
      InsertInEntrySet(it->second, &entries_set_);
      cache_size_ += it->second.GetEntrySize();
    }
  }
  last_write_to_disk_ = base::Time::Now();
  initialized_ = true;
  removed_entries_.clear();

  // The actual IO is asynchronous, so calling WriteToDisk() shouldn't slow down
  // much the merge.
  if (force_index_flush)
    WriteToDisk();
}

void SimpleIndex::WriteToDisk() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return;
  last_write_to_disk_ = base::Time::Now();
  SimpleIndexFile::IndexMetadata index_metadata(entries_set_.size(),
                                                cache_size_);
  scoped_ptr<Pickle> pickle = SimpleIndexFile::Serialize(index_metadata,
                                                         entries_set_);
  cache_thread_->PostTask(FROM_HERE, base::Bind(
      &SimpleIndex::WriteToDiskInternal,
      index_filename_,
      base::Passed(&pickle)));
}

}  // namespace disk_cache
