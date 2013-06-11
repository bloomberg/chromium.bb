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
// Handle Reading/Writing to Cache.

// PNaClTranslationCacheEntry is a shim that provides storage for the
// 'key' and 'data' strings as the disk_cache is performing various async
// operations. It also tracks the open disk_cache::Entry
// and ensures that the entry is closed.
class PNaClTranslationCacheEntry
    : public base::RefCounted<PNaClTranslationCacheEntry> {
 public:
  PNaClTranslationCacheEntry(base::WeakPtr<PNaClTranslationCache> cache,
                             const std::string& key,
                             std::string* read_nexe,
                             const std::string& write_nexe,
                             const CompletionCallback& callback,
                             bool is_read);
  void Start();

  // Writes:                                ---
  //                                        v  |
  // Start -> Open Existing --------------> Write ---> Close
  //                          \              ^
  //                           \             /
  //                            --> Create --
  // Reads:
  // Start -> Open --------Read ----> Close
  //                       |  ^
  //                       |__|
  enum CacheStep {
    UNINITIALIZED,
    OPEN_ENTRY,
    CREATE_ENTRY,
    TRANSFER_ENTRY,
    CLOSE_ENTRY
  };

 private:
  friend class base::RefCounted<PNaClTranslationCacheEntry>;
  ~PNaClTranslationCacheEntry();

  // Try to open an existing entry in the backend
  void OpenEntry();
  // Create a new entry in the backend (for writes)
  void CreateEntry();
  // Write |len| bytes to the backend, starting at |offset|
  void WriteEntry(int offset, int len);
  // Read |len| bytes from the backend, starting at |offset|
  void ReadEntry(int offset, int len);
  // If there was an error, doom the entry. Then post a task to the IO
  // thread to close (and delete) it.
  void CloseEntry(int rv);
  // Call the user callback, and signal to the cache to delete this.
  void Finish(int rv);
  // Used as the callback for all operations to the backend. Handle state
  // transitions, track bytes transferred, and call the other helper methods.
  void DispatchNext(int rv);
  // Get the total transfer size. For reads, must be called after the backend
  // entry has been opened.
  int GetTransferSize();

  base::WeakPtr<PNaClTranslationCache> cache_;

  std::string key_;
  std::string* read_nexe_;
  std::string write_nexe_;
  disk_cache::Entry* entry_;
  CacheStep step_;
  bool is_read_;
  int bytes_transferred_;
  int bytes_to_transfer_;
  scoped_refptr<net::IOBufferWithSize> read_buf_;
  CompletionCallback finish_callback_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(PNaClTranslationCacheEntry);
};

PNaClTranslationCacheEntry::PNaClTranslationCacheEntry(
    base::WeakPtr<PNaClTranslationCache> cache,
    const std::string& key,
    std::string* read_nexe,
    const std::string& write_nexe,
    const CompletionCallback& callback,
    bool is_read)
    : cache_(cache),
      key_(key),
      read_nexe_(read_nexe),
      write_nexe_(write_nexe),
      entry_(NULL),
      step_(UNINITIALIZED),
      is_read_(is_read),
      bytes_transferred_(0),
      bytes_to_transfer_(-1),
      finish_callback_(callback) {}

PNaClTranslationCacheEntry::~PNaClTranslationCacheEntry() {
  // Ensure we have called the user's callback
  DCHECK(finish_callback_.is_null());
}

void PNaClTranslationCacheEntry::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  step_ = OPEN_ENTRY;
  OpenEntry();
}

// OpenEntry, CreateEntry, WriteEntry, ReadEntry and CloseEntry are only called
// from DispatchNext, so they know that cache_ is still valid.
void PNaClTranslationCacheEntry::OpenEntry() {
  int rv = cache_->backend()
      ->OpenEntry(key_,
                  &entry_,
                  base::Bind(&PNaClTranslationCacheEntry::DispatchNext, this));
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PNaClTranslationCacheEntry::CreateEntry() {
  int rv = cache_->backend()->CreateEntry(
      key_,
      &entry_,
      base::Bind(&PNaClTranslationCacheEntry::DispatchNext, this));
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PNaClTranslationCacheEntry::WriteEntry(int offset, int len) {
  scoped_refptr<net::StringIOBuffer> io_buf =
      new net::StringIOBuffer(write_nexe_.substr(offset, len));
  int rv = entry_->WriteData(
      1,
      offset,
      io_buf.get(),
      len,
      base::Bind(&PNaClTranslationCacheEntry::DispatchNext, this),
      false);
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

void PNaClTranslationCacheEntry::ReadEntry(int offset, int len) {
  read_buf_ = new net::IOBufferWithSize(len);
  int rv = entry_->ReadData(
      1,
      offset,
      read_buf_.get(),
      len,
      base::Bind(&PNaClTranslationCacheEntry::DispatchNext, this));
  if (rv != net::ERR_IO_PENDING)
    DispatchNext(rv);
}

int PNaClTranslationCacheEntry::GetTransferSize() {
  if (is_read_) {
    DCHECK(entry_);
    return entry_->GetDataSize(1);
  }
  return write_nexe_.size();
}

void PNaClTranslationCacheEntry::CloseEntry(int rv) {
  DCHECK(entry_);
  if (rv < 0)
    entry_->Doom();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(&CloseDiskCacheEntry, entry_));
  Finish(rv);
}

void PNaClTranslationCacheEntry::Finish(int rv) {
  if (!finish_callback_.is_null()) {
    finish_callback_.Run(rv);
    finish_callback_.Reset();
  }
  cache_->OpComplete(this);
}

void PNaClTranslationCacheEntry::DispatchNext(int rv) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!cache_)
    return;

  switch (step_) {
    case UNINITIALIZED:
      LOG(ERROR) << "Unexpected step in DispatchNext";
      break;

    case OPEN_ENTRY:
      if (rv == net::OK) {
        step_ = TRANSFER_ENTRY;
        bytes_to_transfer_ = GetTransferSize();
        is_read_ ? ReadEntry(0, bytes_to_transfer_)
                 : WriteEntry(0, bytes_to_transfer_);
      } else {
        if (is_read_) {
          // Just a cache miss, not necessarily an error.
          entry_ = NULL;
          Finish(rv);
          break;
        }
        step_ = CREATE_ENTRY;
        CreateEntry();
      }
      break;

    case CREATE_ENTRY:
      if (rv == net::OK) {
        step_ = TRANSFER_ENTRY;
        bytes_to_transfer_ = GetTransferSize();
        WriteEntry(bytes_transferred_, bytes_to_transfer_ - bytes_transferred_);
      } else {
        LOG(ERROR) << "Failed to Create a PNaCl Translation Cache Entry";
        Finish(rv);
      }
      break;

    case TRANSFER_ENTRY:
      if (rv < 0) {
        // We do not call DispatchNext directly if WriteEntry/ReadEntry returns
        // ERR_IO_PENDING, and the callback should not return that value either.
        LOG(ERROR)
            << "Failed to complete write to PNaCl Translation Cache Entry: "
            << rv;
        step_ = CLOSE_ENTRY;
        CloseEntry(rv);
        break;
      } else if (rv > 0) {
        // For reads, copy the data that was just returned
        if (is_read_)
          read_nexe_->append(read_buf_->data(), rv);
        bytes_transferred_ += rv;
        if (bytes_transferred_ < bytes_to_transfer_) {
          int len = bytes_to_transfer_ - bytes_transferred_;
          is_read_ ? ReadEntry(bytes_transferred_, len)
                   : WriteEntry(bytes_transferred_, len);
          break;
        }
      }
      // rv == 0 or we fell through (i.e. we have transferred all the bytes)
      step_ = CLOSE_ENTRY;
      CloseEntry(0);
      break;

    case CLOSE_ENTRY:
      step_ = UNINITIALIZED;
      break;
  }
}

//////////////////////////////////////////////////////////////////////
void PNaClTranslationCache::OpComplete(PNaClTranslationCacheEntry* entry) {
  open_entries_.erase(entry);
}

//////////////////////////////////////////////////////////////////////
// Construction and cache backend initialization
PNaClTranslationCache::PNaClTranslationCache()
    : disk_cache_(NULL), in_memory_(false) {}

PNaClTranslationCache::~PNaClTranslationCache() { delete disk_cache_; }

int PNaClTranslationCache::InitWithDiskBackend(
    const base::FilePath& cache_dir,
    int cache_size,
    const CompletionCallback& callback) {
  return Init(net::DISK_CACHE, cache_dir, cache_size, callback);
}

int PNaClTranslationCache::InitWithMemBackend(
    int cache_size,
    const CompletionCallback& callback) {
  return Init(net::MEMORY_CACHE, base::FilePath(), cache_size, callback);
}

int PNaClTranslationCache::Init(net::CacheType cache_type,
                                const base::FilePath& cache_dir,
                                int cache_size,
                                const CompletionCallback& callback) {
  int rv = disk_cache::CreateCacheBackend(
      cache_type,
      net::CACHE_BACKEND_DEFAULT,
      cache_dir,
      cache_size,
      true /* force_initialize */,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE).get(),
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

void PNaClTranslationCache::StoreNexe(const std::string& key,
                                      const std::string& nexe) {
  StoreNexe(key, nexe, CompletionCallback());
}

void PNaClTranslationCache::StoreNexe(const std::string& key,
                                      const std::string& nexe,
                                      const CompletionCallback& callback) {
  PNaClTranslationCacheEntry* entry = new PNaClTranslationCacheEntry(
      AsWeakPtr(), key, NULL, nexe, callback, false);
  open_entries_[entry] = entry;
  entry->Start();
}

void PNaClTranslationCache::GetNexe(const std::string& key,
                                    std::string* nexe,
                                    const CompletionCallback& callback) {
  PNaClTranslationCacheEntry* entry = new PNaClTranslationCacheEntry(
      AsWeakPtr(), key, nexe, std::string(), callback, true);
  open_entries_[entry] = entry;
  entry->Start();
}

int PNaClTranslationCache::InitCache(const base::FilePath& cache_directory,
                                     bool in_memory,
                                     const CompletionCallback& callback) {
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

}  // namespace pnacl_cache
