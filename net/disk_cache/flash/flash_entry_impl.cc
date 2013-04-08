// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/flash/flash_entry_impl.h"
#include "net/disk_cache/flash/internal_entry.h"

namespace disk_cache {

FlashEntryImpl::FlashEntryImpl(const std::string& key,
                               LogStore* store,
                               base::MessageLoopProxy* cache_thread)
    : init_(false),
      key_(key),
      new_internal_entry_(new InternalEntry(key, store)),
      cache_thread_(cache_thread) {
  memset(stream_sizes_, 0, sizeof(stream_sizes_));
}

FlashEntryImpl::FlashEntryImpl(int32 id,
                               LogStore* store,
                               base::MessageLoopProxy* cache_thread)
    : init_(false),
      old_internal_entry_(new InternalEntry(id, store)),
      cache_thread_(cache_thread) {
}

int FlashEntryImpl::Init(const CompletionCallback& callback) {
  if (new_internal_entry_) {
    DCHECK(callback.is_null());
    init_ = true;
    return net::OK;
  }
  DCHECK(!callback.is_null() && old_internal_entry_);
  callback_ = callback;
  PostTaskAndReplyWithResult(cache_thread_, FROM_HERE,
                             Bind(&InternalEntry::Init, old_internal_entry_),
                             Bind(&FlashEntryImpl::OnInitComplete, this));
  return net::ERR_IO_PENDING;
}

void FlashEntryImpl::Doom() {
  DCHECK(init_);
  NOTREACHED();
}

void FlashEntryImpl::Close() {
  DCHECK(init_);
  Release();
}

std::string FlashEntryImpl::GetKey() const {
  DCHECK(init_);
  return key_;
}

base::Time FlashEntryImpl::GetLastUsed() const {
  DCHECK(init_);
  NOTREACHED();
  return base::Time::Now();
}

base::Time FlashEntryImpl::GetLastModified() const {
  DCHECK(init_);
  NOTREACHED();
  return base::Time::Now();
}

int32 FlashEntryImpl::GetDataSize(int index) const {
  DCHECK(init_);
  return new_internal_entry_->GetDataSize(index);
}

int FlashEntryImpl::ReadData(int index, int offset, IOBuffer* buf, int buf_len,
                             const CompletionCallback& callback) {
  DCHECK(init_);
  return new_internal_entry_->ReadData(index, offset, buf, buf_len, callback);
}

int FlashEntryImpl::WriteData(int index, int offset, IOBuffer* buf, int buf_len,
                              const CompletionCallback& callback,
                              bool truncate) {
  DCHECK(init_);
  return new_internal_entry_->WriteData(index, offset, buf, buf_len, callback);
}

int FlashEntryImpl::ReadSparseData(int64 offset, IOBuffer* buf, int buf_len,
                                   const CompletionCallback& callback) {
  DCHECK(init_);
  NOTREACHED();
  return net::ERR_FAILED;
}

int FlashEntryImpl::WriteSparseData(int64 offset, IOBuffer* buf, int buf_len,
                                    const CompletionCallback& callback) {
  DCHECK(init_);
  NOTREACHED();
  return net::ERR_FAILED;
}

int FlashEntryImpl::GetAvailableRange(int64 offset, int len, int64* start,
                                      const CompletionCallback& callback) {
  DCHECK(init_);
  NOTREACHED();
  return net::ERR_FAILED;
}

bool FlashEntryImpl::CouldBeSparse() const {
  DCHECK(init_);
  NOTREACHED();
  return false;
}

void FlashEntryImpl::CancelSparseIO() {
  DCHECK(init_);
  NOTREACHED();
}

int FlashEntryImpl::ReadyForSparseIO(const CompletionCallback& callback) {
  DCHECK(init_);
  NOTREACHED();
  return net::ERR_FAILED;
}

void FlashEntryImpl::OnInitComplete(
    scoped_ptr<KeyAndStreamSizes> key_and_stream_sizes) {
  DCHECK(!callback_.is_null());
  if (!key_and_stream_sizes) {
    callback_.Run(net::ERR_FAILED);
  } else {
    key_ = key_and_stream_sizes->key;
    memcpy(stream_sizes_, key_and_stream_sizes->stream_sizes,
           sizeof(stream_sizes_));
    init_ = true;
    callback_.Run(net::OK);
  }
}

FlashEntryImpl::~FlashEntryImpl() {
  cache_thread_->PostTask(FROM_HERE,
                          Bind(&InternalEntry::Close, new_internal_entry_));
}

}  // namespace disk_cache
