// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/disk_based_cert_cache.h"

#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

namespace net {

namespace {

// TODO(brandonsalmon): change this number to improve performance.
const size_t kMemoryCacheMaxSize = 30;

// Used to obtain a unique cache key for a certificate in the form of
// "cert:<hash>".
std::string GetCacheKeyForCert(
    const X509Certificate::OSCertHandle cert_handle) {
  SHA1HashValue fingerprint =
      X509Certificate::CalculateFingerprint(cert_handle);

  return "cert:" +
         base::HexEncode(fingerprint.data, arraysize(fingerprint.data));
}

enum CacheResult {
  MEMORY_CACHE_HIT = 0,
  DISK_CACHE_HIT,
  DISK_CACHE_ENTRY_CORRUPT,
  DISK_CACHE_ERROR,
  CACHE_RESULT_MAX
};

void RecordCacheResult(CacheResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "DiskBasedCertCache.CertIoCacheResult", result, CACHE_RESULT_MAX);
}

}  // namespace

// WriteWorkers represent pending SetCertificate jobs in the DiskBasedCertCache.
// Each certificate requested to be stored is assigned a WriteWorker.
// The same certificate should not have multiple WriteWorkers at the same
// time; instead, add a user callback to the existing WriteWorker.
class DiskBasedCertCache::WriteWorker {
 public:
  // |backend| is the backend to store |certificate| in, using
  // |key| as the key for the disk_cache::Entry.
  // |cleanup_callback| is called to clean up this ReadWorker,
  // regardless of success or failure.
  WriteWorker(disk_cache::Backend* backend,
              const std::string& key,
              X509Certificate::OSCertHandle cert_handle,
              const base::Closure& cleanup_callback);

  ~WriteWorker();

  // Writes the given certificate to the cache. On completion, will invoke all
  // user callbacks.
  void Start();

  // Adds a callback to the set of callbacks to be run when this
  // WriteWorker finishes processing.
  void AddCallback(const SetCallback& user_callback);

  // Signals the WriteWorker to abort early. The WriteWorker will be destroyed
  // upon the completion of any pending callbacks. User callbacks will be
  // invoked with an empty string.
  void Cancel();

 private:
  enum State {
    STATE_OPEN,
    STATE_OPEN_COMPLETE,
    STATE_CREATE,
    STATE_CREATE_COMPLETE,
    STATE_WRITE,
    STATE_WRITE_COMPLETE,
    STATE_NONE
  };

  void OnIOComplete(int rv);
  int DoLoop(int rv);

  int DoOpen();
  int DoOpenComplete(int rv);
  int DoCreate();
  int DoCreateComplete(int rv);
  int DoWrite();
  int DoWriteComplete(int rv);

  void Finish(int rv);

  // Invokes all of the |user_callbacks_|
  void RunCallbacks(int rv);

  disk_cache::Backend* backend_;
  const X509Certificate::OSCertHandle cert_handle_;
  std::string key_;
  bool canceled_;

  disk_cache::Entry* entry_;
  State next_state_;
  scoped_refptr<IOBuffer> buffer_;
  int io_buf_len_;

  base::Closure cleanup_callback_;
  std::vector<SetCallback> user_callbacks_;
  CompletionCallback io_callback_;
};

DiskBasedCertCache::WriteWorker::WriteWorker(
    disk_cache::Backend* backend,
    const std::string& key,
    X509Certificate::OSCertHandle cert_handle,
    const base::Closure& cleanup_callback)
    : backend_(backend),
      cert_handle_(X509Certificate::DupOSCertHandle(cert_handle)),
      key_(key),
      canceled_(false),
      entry_(NULL),
      next_state_(STATE_NONE),
      io_buf_len_(0),
      cleanup_callback_(cleanup_callback),
      io_callback_(
          base::Bind(&WriteWorker::OnIOComplete, base::Unretained(this))) {
}

DiskBasedCertCache::WriteWorker::~WriteWorker() {
  if (cert_handle_)
    X509Certificate::FreeOSCertHandle(cert_handle_);
  if (entry_)
    entry_->Close();
}

void DiskBasedCertCache::WriteWorker::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_OPEN;
  int rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING)
    return;

  Finish(rv);
}

void DiskBasedCertCache::WriteWorker::AddCallback(
    const SetCallback& user_callback) {
  user_callbacks_.push_back(user_callback);
}

void DiskBasedCertCache::WriteWorker::Cancel() {
  canceled_ = true;
}

void DiskBasedCertCache::WriteWorker::OnIOComplete(int rv) {
  if (canceled_) {
    Finish(ERR_FAILED);
    return;
  }

  rv = DoLoop(rv);

  if (rv == ERR_IO_PENDING)
    return;

  Finish(rv);
}

int DiskBasedCertCache::WriteWorker::DoLoop(int rv) {
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_OPEN:
        rv = DoOpen();
        break;
      case STATE_OPEN_COMPLETE:
        rv = DoOpenComplete(rv);
        break;
      case STATE_CREATE:
        rv = DoCreate();
        break;
      case STATE_CREATE_COMPLETE:
        rv = DoCreateComplete(rv);
        break;
      case STATE_WRITE:
        rv = DoWrite();
        break;
      case STATE_WRITE_COMPLETE:
        rv = DoWriteComplete(rv);
        break;
      case STATE_NONE:
        NOTREACHED();
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int DiskBasedCertCache::WriteWorker::DoOpen() {
  next_state_ = STATE_OPEN_COMPLETE;
  return backend_->OpenEntry(key_, &entry_, io_callback_);
}

int DiskBasedCertCache::WriteWorker::DoOpenComplete(int rv) {
  // The entry doesn't exist yet, so we should create it.
  if (rv < 0) {
    next_state_ = STATE_CREATE;
    return OK;
  }

  next_state_ = STATE_WRITE;
  return OK;
}

int DiskBasedCertCache::WriteWorker::DoCreate() {
  next_state_ = STATE_CREATE_COMPLETE;
  return backend_->CreateEntry(key_, &entry_, io_callback_);
}

int DiskBasedCertCache::WriteWorker::DoCreateComplete(int rv) {
  if (rv < 0)
    return rv;

  next_state_ = STATE_WRITE;
  return OK;
}

int DiskBasedCertCache::WriteWorker::DoWrite() {
  std::string write_data;
  bool encoded = X509Certificate::GetDEREncoded(cert_handle_, &write_data);

  if (!encoded)
    return ERR_FAILED;

  buffer_ = new IOBuffer(write_data.size());
  io_buf_len_ = write_data.size();
  memcpy(buffer_->data(), write_data.data(), io_buf_len_);

  next_state_ = STATE_WRITE_COMPLETE;

  return entry_->WriteData(0 /* index */,
                           0 /* offset */,
                           buffer_.get(),
                           write_data.size(),
                           io_callback_,
                           true /* truncate */);
}

int DiskBasedCertCache::WriteWorker::DoWriteComplete(int rv) {
  if (rv < io_buf_len_)
    return ERR_FAILED;

  return OK;
}

void DiskBasedCertCache::WriteWorker::Finish(int rv) {
  cleanup_callback_.Run();
  cleanup_callback_.Reset();
  RunCallbacks(rv);
  delete this;
}

void DiskBasedCertCache::WriteWorker::RunCallbacks(int rv) {
  std::string key;
  if (rv >= 0)
    key = key_;

  for (std::vector<SetCallback>::const_iterator it = user_callbacks_.begin();
       it != user_callbacks_.end();
       ++it) {
    it->Run(key);
  }
  user_callbacks_.clear();
}

// ReadWorkers represent pending GetCertificate jobs in the DiskBasedCertCache.
// Each certificate requested to be retrieved from the cache is assigned a
// ReadWorker. The same |key| should not have multiple ReadWorkers at the
// same time; instead, call AddCallback to add a user callback to the
// existing ReadWorker.
class DiskBasedCertCache::ReadWorker {
 public:
  // |backend| is the backend to read |certificate| from, using
  // |key| as the key for the disk_cache::Entry.
  // |cleanup_callback| is called to clean up this ReadWorker,
  // regardless of success or failure.
  ReadWorker(disk_cache::Backend* backend,
             const std::string& key,
             const GetCallback& cleanup_callback);

  ~ReadWorker();

  // Reads the given certificate from the cache. On completion, will invoke all
  // user callbacks.
  void Start();

  // Adds a callback to the set of callbacks to be run when this
  // ReadWorker finishes processing.
  void AddCallback(const GetCallback& user_callback);

  // Signals the ReadWorker to abort early. The ReadWorker will be destroyed
  // upon the completion of any pending callbacks. User callbacks will be
  // invoked with a NULL cert handle.
  void Cancel();

 private:
  enum State {
    STATE_OPEN,
    STATE_OPEN_COMPLETE,
    STATE_READ,
    STATE_READ_COMPLETE,
    STATE_NONE
  };

  void OnIOComplete(int rv);
  int DoLoop(int rv);
  int DoOpen();
  int DoOpenComplete(int rv);
  int DoRead();
  int DoReadComplete(int rv);
  void Finish(int rv);

  // Invokes all of |user_callbacks_|
  void RunCallbacks();

  disk_cache::Backend* backend_;
  X509Certificate::OSCertHandle cert_handle_;
  std::string key_;
  bool canceled_;

  disk_cache::Entry* entry_;

  State next_state_;
  scoped_refptr<IOBuffer> buffer_;
  int io_buf_len_;

  GetCallback cleanup_callback_;
  std::vector<GetCallback> user_callbacks_;
  CompletionCallback io_callback_;
};

DiskBasedCertCache::ReadWorker::ReadWorker(disk_cache::Backend* backend,
                                           const std::string& key,
                                           const GetCallback& cleanup_callback)
    : backend_(backend),
      cert_handle_(NULL),
      key_(key),
      canceled_(false),
      entry_(NULL),
      next_state_(STATE_NONE),
      io_buf_len_(0),
      cleanup_callback_(cleanup_callback),
      io_callback_(
          base::Bind(&ReadWorker::OnIOComplete, base::Unretained(this))) {
}

DiskBasedCertCache::ReadWorker::~ReadWorker() {
  if (entry_)
    entry_->Close();
  if (cert_handle_)
    X509Certificate::FreeOSCertHandle(cert_handle_);
}

void DiskBasedCertCache::ReadWorker::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_OPEN;
  int rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING)
    return;

  Finish(rv);
}

void DiskBasedCertCache::ReadWorker::AddCallback(
    const GetCallback& user_callback) {
  user_callbacks_.push_back(user_callback);
}

void DiskBasedCertCache::ReadWorker::Cancel() {
  canceled_ = true;
}

void DiskBasedCertCache::ReadWorker::OnIOComplete(int rv) {
  if (canceled_) {
    Finish(ERR_FAILED);
    return;
  }

  rv = DoLoop(rv);

  if (rv == ERR_IO_PENDING)
    return;

  Finish(rv);
}

int DiskBasedCertCache::ReadWorker::DoLoop(int rv) {
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_OPEN:
        rv = DoOpen();
        break;
      case STATE_OPEN_COMPLETE:
        rv = DoOpenComplete(rv);
        break;
      case STATE_READ:
        rv = DoRead();
        break;
      case STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case STATE_NONE:
        NOTREACHED();
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int DiskBasedCertCache::ReadWorker::DoOpen() {
  next_state_ = STATE_OPEN_COMPLETE;
  return backend_->OpenEntry(key_, &entry_, io_callback_);
}

int DiskBasedCertCache::ReadWorker::DoOpenComplete(int rv) {
  if (rv < 0) {
    RecordCacheResult(DISK_CACHE_ERROR);
    return rv;
  }

  next_state_ = STATE_READ;
  return OK;
}

int DiskBasedCertCache::ReadWorker::DoRead() {
  next_state_ = STATE_READ_COMPLETE;
  io_buf_len_ = entry_->GetDataSize(0 /* index */);
  buffer_ = new IOBuffer(io_buf_len_);
  return entry_->ReadData(
      0 /* index */, 0 /* offset */, buffer_.get(), io_buf_len_, io_callback_);
}

int DiskBasedCertCache::ReadWorker::DoReadComplete(int rv) {
  // The cache should return the entire buffer length. If it does not,
  // it is probably indicative of an issue other than corruption.
  if (rv < io_buf_len_) {
    RecordCacheResult(DISK_CACHE_ERROR);
    return ERR_FAILED;
  }
  cert_handle_ = X509Certificate::CreateOSCertHandleFromBytes(buffer_->data(),
                                                              io_buf_len_);
  if (!cert_handle_) {
    RecordCacheResult(DISK_CACHE_ENTRY_CORRUPT);
    return ERR_FAILED;
  }

  RecordCacheResult(DISK_CACHE_HIT);
  return OK;
}

void DiskBasedCertCache::ReadWorker::Finish(int rv) {
  cleanup_callback_.Run(cert_handle_);
  cleanup_callback_.Reset();
  RunCallbacks();
  delete this;
}

void DiskBasedCertCache::ReadWorker::RunCallbacks() {
  for (std::vector<GetCallback>::const_iterator it = user_callbacks_.begin();
       it != user_callbacks_.end();
       ++it) {
    it->Run(cert_handle_);
  }
  user_callbacks_.clear();
}

void DiskBasedCertCache::CertFree::operator()(
    X509Certificate::OSCertHandle cert_handle) {
  X509Certificate::FreeOSCertHandle(cert_handle);
}

DiskBasedCertCache::DiskBasedCertCache(disk_cache::Backend* backend)
    : backend_(backend),
      mru_cert_cache_(kMemoryCacheMaxSize),
      mem_cache_hits_(0),
      mem_cache_misses_(0),
      weak_factory_(this) {
  DCHECK(backend_);
}

DiskBasedCertCache::~DiskBasedCertCache() {
  for (WriteWorkerMap::iterator it = write_worker_map_.begin();
       it != write_worker_map_.end();
       ++it) {
    it->second->Cancel();
  }
  for (ReadWorkerMap::iterator it = read_worker_map_.begin();
       it != read_worker_map_.end();
       ++it) {
    it->second->Cancel();
  }
}

void DiskBasedCertCache::GetCertificate(const std::string& key,
                                        const GetCallback& cb) {
  DCHECK(!key.empty());

  // If the handle is already in the MRU cache, just return that (via callback).
  // Note, this will also bring the cert_handle to the front of the recency
  // list in the MRU cache.
  MRUCertCache::iterator mru_it = mru_cert_cache_.Get(key);
  if (mru_it != mru_cert_cache_.end()) {
    RecordCacheResult(MEMORY_CACHE_HIT);
    ++mem_cache_hits_;
    cb.Run(mru_it->second);
    return;
  }
  ++mem_cache_misses_;

  ReadWorkerMap::iterator it = read_worker_map_.find(key);

  if (it == read_worker_map_.end()) {
    ReadWorker* worker =
        new ReadWorker(backend_,
                       key,
                       base::Bind(&DiskBasedCertCache::FinishedReadOperation,
                                  weak_factory_.GetWeakPtr(),
                                  key));
    read_worker_map_[key] = worker;
    worker->AddCallback(cb);
    worker->Start();
  } else {
    it->second->AddCallback(cb);
  }
}

void DiskBasedCertCache::SetCertificate(
    const X509Certificate::OSCertHandle cert_handle,
    const SetCallback& cb) {
  DCHECK(!cb.is_null());
  DCHECK(cert_handle);
  std::string key = GetCacheKeyForCert(cert_handle);

  WriteWorkerMap::iterator it = write_worker_map_.find(key);

  if (it == write_worker_map_.end()) {
    WriteWorker* worker =
        new WriteWorker(backend_,
                        key,
                        cert_handle,
                        base::Bind(&DiskBasedCertCache::FinishedWriteOperation,
                                   weak_factory_.GetWeakPtr(),
                                   key,
                                   cert_handle));
    write_worker_map_[key] = worker;
    worker->AddCallback(cb);
    worker->Start();
  } else {
    it->second->AddCallback(cb);
  }
}

void DiskBasedCertCache::FinishedReadOperation(
    const std::string& key,
    X509Certificate::OSCertHandle cert_handle) {
  if (cert_handle)
    mru_cert_cache_.Put(key, X509Certificate::DupOSCertHandle(cert_handle));
  read_worker_map_.erase(key);
}

void DiskBasedCertCache::FinishedWriteOperation(
    const std::string& key,
    X509Certificate::OSCertHandle cert_handle) {
  write_worker_map_.erase(key);
  if (!key.empty())
    mru_cert_cache_.Put(key, X509Certificate::DupOSCertHandle(cert_handle));
}

}  // namespace net
