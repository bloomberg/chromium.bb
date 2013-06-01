// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_translation_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

using content::BrowserThread;

static const base::FilePath::CharType kDiskCacheDirectoryName[] =
    FILE_PATH_LITERAL("PNaClTranslationCache");

namespace {

void CloseDiskCacheEntry(disk_cache::Entry* entry) { entry->Close(); }

}  // namespace

namespace pnacl_cache {
// These are in pnacl_cache namespace instead of static so they can be used
// by the unit test.
const int kMaxDiskCacheSize = 1000 * 1024 * 1024;
const int kMaxMemCacheSize = 100 * 1024 * 1024;

//////////////////////////////////////////////////////////////////////
// Handle Storing to Cache.

// PNaClTranslationCacheWriteEntry is a shim that provides storage for the
// 'key' and 'data' strings as the disk_cache is performing various async
// operations. It also tracks the open disk_cache::Entry
// and ensures that the entry is closed.
class PNaClTranslationCacheWriteEntry
    : public base::RefCounted<PNaClTranslationCacheWriteEntry> {
 public:
  PNaClTranslationCacheWriteEntry(base::WeakPtr<PNaClTranslationCache> cache,
                                  const std::string& key,
                                  const std::string& nexe,
                                  const net::CompletionCallback& callback);

  void Cache();

  //                                        ---
  //                                        v  |
  // Cache -> Open Existing --------------> Write ---> Close
  //                          \              ^
  //                           \             /
  //                            --> Create --
  enum CacheStep {
    UNINITIALIZED,
    OPEN_ENTRY,
    CREATE_ENTRY,
    WRITE_ENTRY,
    CLOSE_ENTRY
  };

 private:
  friend class base::RefCounted<PNaClTranslationCacheWriteEntry>;
  ~PNaClTranslationCacheWriteEntry();

  void CreateEntry();

  void OpenEntry();

  void WriteEntry(int bytes_to_skip);

  void CloseEntry(int rv);

  void DispatchNext(int rv);

  base::WeakPtr<PNaClTranslationCache> cache_;

  std::string key_;
  std::string nexe_;
  disk_cache::Entry* entry_;
  CacheStep step_;
  CompletionCallback finish_callback_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(PNaClTranslationCacheWriteEntry);
};

PNaClTranslationCacheWriteEntry::PNaClTranslationCacheWriteEntry(
    base::WeakPtr<PNaClTranslationCache> cache,
    const std::string& key,
    const std::string& nexe,
    const net::CompletionCallback& callback)
    : cache_(cache),
      key_(key),
      nexe_(nexe),
      entry_(NULL),
      step_(UNINITIALIZED),
      finish_callback_(callback) {}

PNaClTranslationCacheWriteEntry::~PNaClTranslationCacheWriteEntry() {
  if (entry_)
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(&CloseDiskCacheEntry, entry_));
}

void PNaClTranslationCacheWriteEntry::Cache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  step_ = OPEN_ENTRY;
  OpenEntry();
}

// OpenEntry, CreateEntry, WriteEntry, and CloseEntry are only called from
// DispatchNext, so they know that cache_ is still valid.
void PNaClTranslationCacheWriteEntry::OpenEntry() {
  int rv = cache_->backend()->OpenEntry(
      key_,
      &entry_,
      base::Bind(&PNaClTranslationCacheWriteEntry::DispatchNext, this));
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PNaClTranslationCacheWriteEntry::CreateEntry() {
  int rv = cache_->backend()->CreateEntry(
      key_,
      &entry_,
      base::Bind(&PNaClTranslationCacheWriteEntry::DispatchNext, this));
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PNaClTranslationCacheWriteEntry::WriteEntry(int bytes_to_skip) {
  nexe_ = nexe_.substr(bytes_to_skip);
  scoped_refptr<net::StringIOBuffer> io_buf = new net::StringIOBuffer(nexe_);
  int rv = entry_->WriteData(
      1,
      0,
      io_buf,
      nexe_.length(),
      base::Bind(&PNaClTranslationCacheWriteEntry::DispatchNext, this),
      false);
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PNaClTranslationCacheWriteEntry::CloseEntry(int rv) {
  if (rv < 0)
    entry_->Doom();
  if (!finish_callback_.is_null()) {
    finish_callback_.Run(rv);
    finish_callback_.Reset();
  }
  cache_->WriteComplete(this);
}

void PNaClTranslationCacheWriteEntry::DispatchNext(int rv) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!cache_)
    return;

  switch (step_) {
    case UNINITIALIZED:
      LOG(ERROR) << "Unexpected step in DispatchNext";
      break;

    case OPEN_ENTRY:
      if (rv == net::OK) {
        step_ = WRITE_ENTRY;
        WriteEntry(0);
      } else {
        step_ = CREATE_ENTRY;
        CreateEntry();
      }
      break;

    case CREATE_ENTRY:
      if (rv == net::OK) {
        step_ = WRITE_ENTRY;
        WriteEntry(0);
      } else {
        LOG(ERROR) << "Failed to Open/Create a PNaCl Translation Cache Entry";
        CloseEntry(rv);
      }
      break;

    case WRITE_ENTRY:
      if (rv < 0) {
        // We do not call DispatchNext directly if WriteEntry returns
        // ERR_IO_PENDING, and the callback should not return that value either.
        LOG(ERROR)
            << "Failed to complete write to PNaCl Translation Cache Entry: "
            << rv;
        step_ = CLOSE_ENTRY;
        CloseEntry(rv);
        break;
      }
      if (rv == 0) {
        step_ = CLOSE_ENTRY;
        CloseEntry(rv);
        break;
      }
      // rv bytes were written; call WriteEntry again to skip them and try to
      // write the rest.
      WriteEntry(rv);
      break;

    case CLOSE_ENTRY:
      step_ = UNINITIALIZED;
      break;
  }
}

//////////////////////////////////////////////////////////////////////
void PNaClTranslationCache::WriteComplete(
    PNaClTranslationCacheWriteEntry* entry) {
  write_entries_.erase(entry);
}

//////////////////////////////////////////////////////////////////////
// Construction and cache backend initialization
PNaClTranslationCache::PNaClTranslationCache()
    : disk_cache_(NULL), in_memory_(false) {}

PNaClTranslationCache::~PNaClTranslationCache() {}

int PNaClTranslationCache::InitWithDiskBackend(
    const base::FilePath& cache_dir,
    int cache_size,
    const net::CompletionCallback& callback) {
  return Init(net::DISK_CACHE, cache_dir, cache_size, callback);
}

int PNaClTranslationCache::InitWithMemBackend(
    int cache_size,
    const net::CompletionCallback& callback) {
  return Init(net::MEMORY_CACHE, base::FilePath(), cache_size, callback);
}

int PNaClTranslationCache::Init(net::CacheType cache_type,
                                const base::FilePath& cache_dir,
                                int cache_size,
                                const net::CompletionCallback& callback) {
  int rv = disk_cache::CreateCacheBackend(
      cache_type,
      net::CACHE_BACKEND_DEFAULT,
      cache_dir,
      cache_size,
      true /* force_initialize */,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE),
      NULL, /* dummy net log */
      &disk_cache_,
      base::Bind(&PNaClTranslationCache::OnCreateBackendComplete, AsWeakPtr()));
  init_callback_ = callback;
  if (rv != net::ERR_IO_PENDING) {
    OnCreateBackendComplete(rv);
  }
  return rv;
}

void PNaClTranslationCache::OnCreateBackendComplete(int rv) {
  // Invoke our client's callback function.
  if (!init_callback_.is_null()) {
    init_callback_.Run(rv);
    init_callback_.Reset();
  }
}

//////////////////////////////////////////////////////////////////////
// High-level API

// TODO(dschuff): Surely there must be a way to just create a null callback?
static void NullCallback(int ignored) {}

void PNaClTranslationCache::StoreNexe(const std::string& key,
                                      const std::string& nexe) {
  StoreNexe(key, nexe, base::Bind(NullCallback));
}

void PNaClTranslationCache::StoreNexe(const std::string& key,
                                      const std::string& nexe,
                                      const net::CompletionCallback& callback) {
  PNaClTranslationCacheWriteEntry* entry =
      new PNaClTranslationCacheWriteEntry(AsWeakPtr(), key, nexe, callback);
  write_entries_[entry] = entry;
  entry->Cache();
}

int PNaClTranslationCache::GetNexe(const std::string& key,
                                   std::string* nexe,
                                   const net::CompletionCallback& callback) {
  // TODO(dschuff): Actually find the entry, and do the right thing.
  // Shader cache ended up making a separate ReadHelper, analogous
  // to the PNaClTranslationCacheWriteEntry.
  return net::OK;
}

int PNaClTranslationCache::InitCache(const base::FilePath& cache_directory,
                                     bool in_memory,
                                     const net::CompletionCallback& callback) {
  int rv;
  in_memory_ = in_memory;
  if (in_memory_) {
    rv = InitWithMemBackend(kMaxMemCacheSize, callback);
  } else {
    rv = InitWithDiskBackend(cache_directory.Append(kDiskCacheDirectoryName),
                             kMaxDiskCacheSize,
                             callback);
  }

  return rv;
}

int PNaClTranslationCache::Size() {
  if (!disk_cache_)
    return -1;
  return disk_cache_->GetEntryCount();
}

}  // namespace nacl_cache
