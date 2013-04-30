// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_backend_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"

using base::Closure;
using base::FilePath;
using base::MessageLoopProxy;
using base::SingleThreadTaskRunner;
using base::Time;
using base::WorkerPool;
using file_util::DirectoryExists;
using file_util::CreateDirectory;

namespace {

// Cache size when all other size heuristics failed.
const uint64 kDefaultCacheSize = 80 * 1024 * 1024;

// Must run on IO Thread.
void DeleteBackendImpl(disk_cache::Backend** backend,
                       const net::CompletionCallback& callback,
                       int result) {
  DCHECK(*backend);
  delete *backend;
  *backend = NULL;
  callback.Run(result);
}

// Detects if the files in the cache directory match the current disk cache
// backend type and version. If the directory contains no cache, occupies it
// with the fresh structure.
//
// There is a convention among disk cache backends: looking at the magic in the
// file "index" it should be sufficient to determine if the cache belongs to the
// currently running backend. The Simple Backend stores its index in the file
// "the-real-index" (see simple_index.cc) and the file "index" only signifies
// presence of the implementation's magic and version. There are two reasons for
// that:
// 1. Absence of the index is itself not a fatal error in the Simple Backend
// 2. The Simple Backend has pickled file format for the index making it hacky
//    to have the magic in the right place.
bool FileStructureConsistent(const base::FilePath& path) {
  if (!file_util::PathExists(path) && !file_util::CreateDirectory(path)) {
    LOG(ERROR) << "Failed to create directory: " << path.LossyDisplayName();
    return false;
  }
  const base::FilePath fake_index = path.AppendASCII("index");
  base::PlatformFileError error;
  base::PlatformFile fake_index_file = base::CreatePlatformFile(
      fake_index,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &error);
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    base::PlatformFile file = base::CreatePlatformFile(
        fake_index,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
        NULL, &error);
    disk_cache::SimpleFileHeader file_contents;
    file_contents.initial_magic_number = disk_cache::kSimpleInitialMagicNumber;
    file_contents.version = disk_cache::kSimpleVersion;
    int bytes_written = base::WritePlatformFile(
        file, 0, reinterpret_cast<char*>(&file_contents),
        sizeof(file_contents));
    if (!base::ClosePlatformFile(file) ||
        bytes_written != sizeof(file_contents)) {
      LOG(ERROR) << "Failed to write cache structure file: "
                 << path.LossyDisplayName();
      return false;
    }
    return true;
  } else if (error != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Could not open cache structure file: "
               << path.LossyDisplayName();
    return false;
  } else {
    disk_cache::SimpleFileHeader file_header;
    int bytes_read = base::ReadPlatformFile(
        fake_index_file, 0, reinterpret_cast<char*>(&file_header),
        sizeof(file_header));
    if (!base::ClosePlatformFile(fake_index_file) ||
        bytes_read != sizeof(file_header) ||
        file_header.initial_magic_number !=
            disk_cache::kSimpleInitialMagicNumber ||
        file_header.version != disk_cache::kSimpleVersion) {
      LOG(ERROR) << "File structure does not match the disk cache backend.";
      return false;
    }
    return true;
  }
}

void CallCompletionCallback(const net::CompletionCallback& callback,
                            scoped_ptr<int> result) {
  DCHECK(!callback.is_null());
  DCHECK(result);
  callback.Run(*result);
}

void RecordIndexLoad(base::TimeTicks constructed_since, int result) {
  const base::TimeDelta creation_to_index = base::TimeTicks::Now() -
                                            constructed_since;
  if (result == net::OK)
    UMA_HISTOGRAM_TIMES("SimpleCache.CreationToIndex", creation_to_index);
  else
    UMA_HISTOGRAM_TIMES("SimpleCache.CreationToIndexFail", creation_to_index);
}

}  // namespace

namespace disk_cache {

SimpleBackendImpl::SimpleBackendImpl(
    const FilePath& path,
    int max_bytes,
    net::CacheType type,
    base::SingleThreadTaskRunner* cache_thread,
    net::NetLog* net_log)
    : path_(path),
      index_(new SimpleIndex(cache_thread,
                             MessageLoopProxy::current(),  // io_thread
                             path)),
      cache_thread_(cache_thread),
      orig_max_size_(max_bytes) {
  index_->ExecuteWhenReady(base::Bind(&RecordIndexLoad,
                                      base::TimeTicks::Now()));
}

SimpleBackendImpl::~SimpleBackendImpl() {
  index_->WriteToDisk();
}

int SimpleBackendImpl::Init(const CompletionCallback& completion_callback) {
  InitializeIndexCallback initialize_index_callback =
      base::Bind(&SimpleBackendImpl::InitializeIndex,
                 base::Unretained(this),
                 completion_callback);
  cache_thread_->PostTask(
      FROM_HERE,
      base::Bind(&SimpleBackendImpl::ProvideDirectorySuggestBetterCacheSize,
                 MessageLoopProxy::current(),  // io_thread
                 path_,
                 initialize_index_callback,
                 orig_max_size_));
  return net::ERR_IO_PENDING;
}

bool SimpleBackendImpl::SetMaxSize(int max_bytes) {
  orig_max_size_ = max_bytes;
  return index_->SetMaxSize(max_bytes);
}

void SimpleBackendImpl::OnDeactivated(const SimpleEntryImpl* entry) {
  DCHECK_LT(0U, active_entries_.count(entry->entry_hash()));
  active_entries_.erase(entry->entry_hash());
}

net::CacheType SimpleBackendImpl::GetCacheType() const {
  return net::DISK_CACHE;
}

int32 SimpleBackendImpl::GetEntryCount() const {
  // TODO(pasko): Use directory file count when index is not ready.
  return index_->GetEntryCount();
}

int SimpleBackendImpl::OpenEntry(const std::string& key,
                                 Entry** entry,
                                 const CompletionCallback& callback) {
  scoped_refptr<SimpleEntryImpl> simple_entry = CreateOrFindActiveEntry(key);
  return simple_entry->OpenEntry(entry, callback);
}

int SimpleBackendImpl::CreateEntry(const std::string& key,
                                   Entry** entry,
                                   const CompletionCallback& callback) {
  scoped_refptr<SimpleEntryImpl> simple_entry = CreateOrFindActiveEntry(key);
  return simple_entry->CreateEntry(entry, callback);
}

int SimpleBackendImpl::DoomEntry(const std::string& key,
                                 const net::CompletionCallback& callback) {
  scoped_refptr<SimpleEntryImpl> simple_entry = CreateOrFindActiveEntry(key);
  return simple_entry->DoomEntry(callback);
}

int SimpleBackendImpl::DoomAllEntries(const CompletionCallback& callback) {
  return DoomEntriesBetween(Time(), Time(), callback);
}

void SimpleBackendImpl::IndexReadyForDoom(Time initial_time,
                                          Time end_time,
                                          const CompletionCallback& callback,
                                          int result) {
  if (result != net::OK) {
    callback.Run(result);
    return;
  }
  scoped_ptr<std::vector<uint64> > removed_key_hashes(
      index_->RemoveEntriesBetween(initial_time, end_time).release());

  // If any of the entries we are dooming are currently open, we need to remove
  // them from |active_entries_|, so that attempts to create new entries will
  // succeed and attempts to open them will fail.
  for (int i = removed_key_hashes->size() - 1; i >= 0; --i) {
    const uint64 entry_hash = (*removed_key_hashes)[i];
    EntryMap::iterator it = active_entries_.find(entry_hash);
    if (it == active_entries_.end())
      continue;
    SimpleEntryImpl* entry = it->second;
    entry->Doom();

    (*removed_key_hashes)[i] = removed_key_hashes->back();
    removed_key_hashes->resize(removed_key_hashes->size() - 1);
  }

  scoped_ptr<int> new_result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::DoomEntrySet,
                            base::Passed(&removed_key_hashes), path_,
                            new_result.get());
  Closure reply = base::Bind(&CallCompletionCallback,
                             callback, base::Passed(&new_result));
  WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
}

int SimpleBackendImpl::DoomEntriesBetween(
    const Time initial_time,
    const Time end_time,
    const CompletionCallback& callback) {
  return index_->ExecuteWhenReady(
      base::Bind(&SimpleBackendImpl::IndexReadyForDoom, AsWeakPtr(),
                 initial_time, end_time, callback));
}

int SimpleBackendImpl::DoomEntriesSince(
    const Time initial_time,
    const CompletionCallback& callback) {
  return DoomEntriesBetween(initial_time, Time(), callback);
}

int SimpleBackendImpl::OpenNextEntry(void** iter,
                                     Entry** next_entry,
                                     const CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

void SimpleBackendImpl::EndEnumeration(void** iter) {
  NOTIMPLEMENTED();
}

void SimpleBackendImpl::GetStats(
    std::vector<std::pair<std::string, std::string> >* stats) {
  NOTIMPLEMENTED();
}

void SimpleBackendImpl::OnExternalCacheHit(const std::string& key) {
  index_->UseIfExists(key);
}

void SimpleBackendImpl::InitializeIndex(
    const CompletionCallback& callback, uint64 suggested_max_size, int result) {
  if (result == net::OK) {
    index_->SetMaxSize(suggested_max_size);
    index_->Initialize();
  }
  callback.Run(result);
}

// static
void SimpleBackendImpl::ProvideDirectorySuggestBetterCacheSize(
    SingleThreadTaskRunner* io_thread,
    const base::FilePath& path,
    const InitializeIndexCallback& initialize_index_callback,
    uint64 suggested_max_size) {
  int rv = net::OK;
  uint64 max_size = suggested_max_size;
  if (!FileStructureConsistent(path)) {
    LOG(ERROR) << "Simple Cache Backend: wrong file structure on disk: "
               << path.LossyDisplayName();
    rv = net::ERR_FAILED;
  } else {
    if (!max_size) {
      int64 available = base::SysInfo::AmountOfFreeDiskSpace(path);
      if (available < 0)
        max_size = kDefaultCacheSize;
      else
        // TODO(pasko): Move PreferedCacheSize() to cache_util.h. Also fix the
        // spelling.
        max_size = PreferedCacheSize(available);
    }
    DCHECK(max_size);
  }
  io_thread->PostTask(FROM_HERE,
                      base::Bind(initialize_index_callback, max_size, rv));
}

scoped_refptr<SimpleEntryImpl> SimpleBackendImpl::CreateOrFindActiveEntry(
    const std::string& key) {
  const uint64 entry_hash = simple_util::GetEntryHashKey(key);

  std::pair<EntryMap::iterator, bool> insert_result =
      active_entries_.insert(std::make_pair(entry_hash,
                                            base::WeakPtr<SimpleEntryImpl>()));
  EntryMap::iterator& it = insert_result.first;
  if (insert_result.second)
    DCHECK(!it->second);
  if (!it->second) {
    SimpleEntryImpl* entry = new SimpleEntryImpl(this, path_, key, entry_hash);
    it->second = entry->AsWeakPtr();
  }
  DCHECK(it->second);
  // It's possible, but unlikely, that we have an entry hash collision with a
  // currently active entry.
  if (key != it->second->key()) {
    it->second->Doom();
    DCHECK_EQ(0U, active_entries_.count(entry_hash));
    return CreateOrFindActiveEntry(key);
  }
  return make_scoped_refptr(it->second.get());
}

}  // namespace disk_cache
