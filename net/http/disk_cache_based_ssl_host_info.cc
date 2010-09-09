// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/disk_cache_based_ssl_host_info.h"

#include "base/callback.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache.h"

namespace net {

DiskCacheBasedSSLHostInfo::DiskCacheBasedSSLHostInfo(
    const std::string& hostname, HttpCache* http_cache)
    : callback_(NewCallback(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                            &DiskCacheBasedSSLHostInfo::DoLoop)),
      state_(GET_BACKEND),
      ready_(false),
      hostname_(hostname),
      http_cache_(http_cache),
      backend_(NULL),
      entry_(NULL),
      user_callback_(NULL) {
  // We need to make sure that we aren't deleted while a callback is
  // outstanding. This reference is balanced in |WaitForDataReadyDone|.
  this->AddRef();
  DoLoop(OK);
}

DiskCacheBasedSSLHostInfo::~DiskCacheBasedSSLHostInfo() {
  DCHECK(!user_callback_);
  DCHECK(!entry_);
}

std::string DiskCacheBasedSSLHostInfo::key() const {
  return "sslhostinfo:" + hostname_;
}

void DiskCacheBasedSSLHostInfo::DoLoop(int rv) {
  switch (state_) {
    case GET_BACKEND:
      return DoGetBackend();
    case GET_BACKEND_COMPLETE:
      return DoGetBackendComplete(rv);
    case OPEN:
      return DoOpen();
    case OPEN_COMPLETE:
      return DoOpenComplete(rv);
    case READ:
      return DoRead();
    case READ_COMPLETE:
      return DoReadComplete(rv);
    case CREATE:
      return DoCreate();
    case CREATE_COMPLETE:
      return DoCreateComplete(rv);
    case WRITE:
      return DoWrite();
    case WRITE_COMPLETE:
      return DoWriteComplete(rv);
    default:
      NOTREACHED();
  }
}

void DiskCacheBasedSSLHostInfo::DoGetBackend() {
  int rv = http_cache_->GetBackend(&backend_, callback_.get());
  state_ = GET_BACKEND_COMPLETE;

  if (rv == ERR_IO_PENDING) {
    return;
  } else if (rv != OK) {
    WaitForDataReadyDone();
  } else {
    DoLoop(OK);
  }
}

void DiskCacheBasedSSLHostInfo::DoGetBackendComplete(int rv) {
  if (rv == OK) {
    state_ = OPEN;
    DoLoop(OK);
  } else {
    WaitForDataReadyDone();
  }
}

void DiskCacheBasedSSLHostInfo::DoOpen() {
  int rv = backend_->OpenEntry(key(), &entry_, callback_.get());
  state_ = OPEN_COMPLETE;

  if (rv == ERR_IO_PENDING) {
    return;
  } else if (rv != OK) {
    WaitForDataReadyDone();
  } else {
    DoLoop(OK);
  }
}

void DiskCacheBasedSSLHostInfo::DoOpenComplete(int rv) {
  if (rv == OK) {
    state_ = READ;
    DoLoop(OK);
  } else {
    WaitForDataReadyDone();
  }
}

void DiskCacheBasedSSLHostInfo::DoRead() {
  const int32 size = entry_->GetDataSize(0 /* index */);
  if (!size) {
    WaitForDataReadyDone();
    return;
  }

  read_buffer_ = new IOBuffer(size);
  int rv = entry_->ReadData(0 /* index */, 0 /* offset */, read_buffer_,
                            size, callback_.get());
  if (rv == ERR_IO_PENDING) {
    state_ = READ_COMPLETE;
    return;
  }

  DoReadComplete(rv);
}

void DiskCacheBasedSSLHostInfo::DoReadComplete(int rv) {
  if (rv > 0)
    data_ = std::string(read_buffer_->data(), rv);

  WaitForDataReadyDone();
}

void DiskCacheBasedSSLHostInfo::WaitForDataReadyDone() {
  CompletionCallback* callback;

  DCHECK(!ready_);
  state_ = NONE;
  ready_ = true;
  callback = user_callback_;
  user_callback_ = NULL;
  // We close the entry because, if we shutdown before ::Set is called, then we
  // might leak a cache reference, which causes a DCHECK on shutdown.
  if (entry_)
    entry_->Close();
  entry_ = NULL;

  if (callback)
    callback->Run(OK);

  this->Release();  // balances the AddRef in the constructor.
}

int DiskCacheBasedSSLHostInfo::WaitForDataReady(CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  if (ready_)
    return OK;
  if (callback) {
    DCHECK(!user_callback_);
    user_callback_ = callback;
  }
  return ERR_IO_PENDING;
}

void DiskCacheBasedSSLHostInfo::Set(const std::string& new_data) {
  DCHECK(CalledOnValidThread());

  if (new_data.empty())
    return;

  DCHECK(new_data_.empty());
  CHECK(ready_);
  DCHECK(user_callback_ == NULL);
  new_data_ = new_data;

  if (!backend_)
    return;

  this->AddRef();  // we don't want to be deleted while the callback is running.
  state_ = CREATE;
  DoLoop(OK);
}

void DiskCacheBasedSSLHostInfo::DoCreate() {
  DCHECK(entry_ == NULL);
  int rv = backend_->CreateEntry(key(), &entry_, callback_.get());
  state_ = CREATE_COMPLETE;
  if (rv == ERR_IO_PENDING)
    return;
  if (rv != OK)
    SetDone();

  DoLoop(OK);
}

void DiskCacheBasedSSLHostInfo::DoCreateComplete(int rv) {
  if (rv != OK) {
    SetDone();
    return;
  }
  state_ = WRITE;
  DoLoop(OK);
}

void DiskCacheBasedSSLHostInfo::DoWrite() {
  write_buffer_ = new IOBuffer(new_data_.size());
  memcpy(write_buffer_->data(), new_data_.data(), new_data_.size());
  int rv = entry_->WriteData(0 /* index */, 0 /* offset */, write_buffer_,
                             new_data_.size(), callback_.get(),
                             true /* truncate */);
  state_ = WRITE_COMPLETE;
  if (rv == ERR_IO_PENDING)
    return;
  if (rv != OK) {
    SetDone();
    return;
  }

  DoLoop(OK);
}

void DiskCacheBasedSSLHostInfo::DoWriteComplete(int rv) {
  SetDone();
}

void DiskCacheBasedSSLHostInfo::SetDone() {
  if (entry_)
    entry_->Close();
  entry_ = NULL;
  this->Release();  // matches up with AddRef in |Set|.
}

} // namespace net
