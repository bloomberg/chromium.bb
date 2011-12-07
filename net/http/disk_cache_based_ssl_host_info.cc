// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/disk_cache_based_ssl_host_info.h"

#include "base/callback.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"

namespace net {

DiskCacheBasedSSLHostInfo::CallbackImpl::CallbackImpl(
    const base::WeakPtr<DiskCacheBasedSSLHostInfo>& obj,
    void (DiskCacheBasedSSLHostInfo::*meth)(int))
    : obj_(obj),
      meth_(meth),
      backend_(NULL),
      entry_(NULL) {
}

DiskCacheBasedSSLHostInfo::CallbackImpl::~CallbackImpl() {}

void DiskCacheBasedSSLHostInfo::CallbackImpl::RunWithParams(
    const Tuple1<int>& params) {
  if (!obj_) {
    delete this;
  } else {
    DispatchToMethod(obj_.get(), meth_, params);
  }
}

DiskCacheBasedSSLHostInfo::DiskCacheBasedSSLHostInfo(
    const std::string& hostname,
    const SSLConfig& ssl_config,
    CertVerifier* cert_verifier,
    HttpCache* http_cache)
    : SSLHostInfo(hostname, ssl_config, cert_verifier),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      callback_(new CallbackImpl(weak_ptr_factory_.GetWeakPtr(),
                                 &DiskCacheBasedSSLHostInfo::OnIOComplete)),
      state_(GET_BACKEND),
      ready_(false),
      found_entry_(false),
      hostname_(hostname),
      http_cache_(http_cache),
      backend_(NULL),
      entry_(NULL) {
}

void DiskCacheBasedSSLHostInfo::Start() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(GET_BACKEND, state_);
  DoLoop(OK);
}

int DiskCacheBasedSSLHostInfo::WaitForDataReady(
    const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ != GET_BACKEND);

  if (ready_)
    return OK;

  if (!callback.is_null()) {
    DCHECK(user_callback_.is_null());
    user_callback_ = callback;
  }

  return ERR_IO_PENDING;
}

void DiskCacheBasedSSLHostInfo::Persist() {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ != GET_BACKEND);

  DCHECK(new_data_.empty());
  CHECK(ready_);
  DCHECK(user_callback_.is_null());
  new_data_ = Serialize();

  if (!backend_)
    return;

  state_ = CREATE_OR_OPEN;
  DoLoop(OK);
}

DiskCacheBasedSSLHostInfo::~DiskCacheBasedSSLHostInfo() {
  DCHECK(user_callback_.is_null());
  if (entry_)
    entry_->Close();
  if (!IsCallbackPending())
    delete callback_;
}

std::string DiskCacheBasedSSLHostInfo::key() const {
  return "sslhostinfo:" + hostname_;
}

void DiskCacheBasedSSLHostInfo::OnIOComplete(int rv) {
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !user_callback_.is_null()) {
    CompletionCallback callback = user_callback_;
    user_callback_.Reset();
    callback.Run(rv);
  }
}

int DiskCacheBasedSSLHostInfo::DoLoop(int rv) {
  do {
    switch (state_) {
      case GET_BACKEND:
        rv = DoGetBackend();
        break;
      case GET_BACKEND_COMPLETE:
        rv = DoGetBackendComplete(rv);
        break;
      case OPEN:
        rv = DoOpen();
        break;
      case OPEN_COMPLETE:
        rv = DoOpenComplete(rv);
        break;
      case READ:
        rv = DoRead();
        break;
      case READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case WAIT_FOR_DATA_READY_DONE:
        rv = DoWaitForDataReadyDone();
        break;
      case CREATE_OR_OPEN:
        rv = DoCreateOrOpen();
        break;
      case CREATE_OR_OPEN_COMPLETE:
        rv = DoCreateOrOpenComplete(rv);
        break;
      case WRITE:
        rv = DoWrite();
        break;
      case WRITE_COMPLETE:
        rv = DoWriteComplete(rv);
        break;
      case SET_DONE:
        rv = DoSetDone();
        break;
      default:
        rv = OK;
        NOTREACHED();
    }
  } while (rv != ERR_IO_PENDING && state_ != NONE);

  return rv;
}

int DiskCacheBasedSSLHostInfo::DoGetBackendComplete(int rv) {
  if (rv == OK) {
    backend_ = callback_->backend();
    state_ = OPEN;
  } else {
    state_ = WAIT_FOR_DATA_READY_DONE;
  }
  return OK;
}

int DiskCacheBasedSSLHostInfo::DoOpenComplete(int rv) {
  if (rv == OK) {
    entry_ = callback_->entry();
    state_ = READ;
    found_entry_ = true;
  } else {
    state_ = WAIT_FOR_DATA_READY_DONE;
  }

  return OK;
}

int DiskCacheBasedSSLHostInfo::DoReadComplete(int rv) {
  if (rv > 0)
    data_.assign(read_buffer_->data(), rv);

  state_ = WAIT_FOR_DATA_READY_DONE;
  return OK;
}

int DiskCacheBasedSSLHostInfo::DoWriteComplete(int rv) {
  state_ = SET_DONE;
  return OK;
}

int DiskCacheBasedSSLHostInfo::DoCreateOrOpenComplete(int rv) {
  if (rv != OK) {
    state_ = SET_DONE;
  } else {
    entry_ = callback_->entry();
    state_ = WRITE;
  }
  return OK;
}

int DiskCacheBasedSSLHostInfo::DoGetBackend() {
  state_ = GET_BACKEND_COMPLETE;
  return http_cache_->GetBackend(callback_->backend_pointer(), callback_);
}

int DiskCacheBasedSSLHostInfo::DoOpen() {
  state_ = OPEN_COMPLETE;
  return backend_->OpenEntry(key(), callback_->entry_pointer(), callback_);
}

int DiskCacheBasedSSLHostInfo::DoRead() {
  const int32 size = entry_->GetDataSize(0 /* index */);
  if (!size) {
    state_ = WAIT_FOR_DATA_READY_DONE;
    return OK;
  }

  read_buffer_ = new IOBuffer(size);
  state_ = READ_COMPLETE;
  return entry_->ReadData(0 /* index */, 0 /* offset */, read_buffer_,
                          size, callback_);
}

int DiskCacheBasedSSLHostInfo::DoWrite() {
  write_buffer_ = new IOBuffer(new_data_.size());
  memcpy(write_buffer_->data(), new_data_.data(), new_data_.size());
  state_ = WRITE_COMPLETE;

  return entry_->WriteData(0 /* index */, 0 /* offset */, write_buffer_,
                           new_data_.size(), callback_, true /* truncate */);
}

int DiskCacheBasedSSLHostInfo::DoCreateOrOpen() {
  DCHECK(entry_ == NULL);
  state_ = CREATE_OR_OPEN_COMPLETE;
  if (found_entry_)
    return backend_->OpenEntry(key(), callback_->entry_pointer(), callback_);

  return backend_->CreateEntry(key(), callback_->entry_pointer(), callback_);
}

int DiskCacheBasedSSLHostInfo::DoWaitForDataReadyDone() {
  DCHECK(!ready_);
  state_ = NONE;
  ready_ = true;
  // We close the entry because, if we shutdown before ::Persist is called,
  // then we might leak a cache reference, which causes a DCHECK on shutdown.
  if (entry_)
    entry_->Close();
  entry_ = NULL;
  Parse(data_);
  return OK;
}

int DiskCacheBasedSSLHostInfo::DoSetDone() {
  if (entry_)
    entry_->Close();
  entry_ = NULL;
  state_ = NONE;
  return OK;
}

bool DiskCacheBasedSSLHostInfo::IsCallbackPending() const {
  switch (state_) {
    case GET_BACKEND_COMPLETE:
    case OPEN_COMPLETE:
    case READ_COMPLETE:
    case CREATE_OR_OPEN_COMPLETE:
    case WRITE_COMPLETE:
      return true;
    default:
      return false;
  }
}

}  // namespace net
