// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_entry_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/threading/worker_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"

namespace {

typedef disk_cache::Entry::CompletionCallback CompletionCallback;
typedef disk_cache::SimpleSynchronousEntry::SynchronousCreationCallback
    SynchronousCreationCallback;
typedef disk_cache::SimpleSynchronousEntry::SynchronousOperationCallback
    SynchronousOperationCallback;

}  // namespace

namespace disk_cache {

using base::FilePath;
using base::MessageLoopProxy;
using base::Time;
using base::WorkerPool;

// static
int SimpleEntryImpl::OpenEntry(SimpleIndex* index,
                               const FilePath& path,
                               const std::string& key,
                               Entry** entry,
                               const CompletionCallback& callback) {
  // TODO(gavinp): More closely unify the last_used_ in the
  // SimpleSynchronousEntry  and the SimpleIndex.
  if (!index || index->UseIfExists(key)) {
    scoped_refptr<SimpleEntryImpl> new_entry =
        new SimpleEntryImpl(index, path, key);
    SynchronousCreationCallback sync_creation_callback =
        base::Bind(&SimpleEntryImpl::CreationOperationComplete,
                   new_entry, entry, callback);
    WorkerPool::PostTask(FROM_HERE,
                         base::Bind(&SimpleSynchronousEntry::OpenEntry, path,
                                    key, MessageLoopProxy::current(),
                                    sync_creation_callback),
                         true);
    return net::ERR_IO_PENDING;
  }
  return net::ERR_FAILED;
}

// static
int SimpleEntryImpl::CreateEntry(SimpleIndex* index,
                                 const FilePath& path,
                                 const std::string& key,
                                 Entry** entry,
                                 const CompletionCallback& callback) {
  scoped_refptr<SimpleEntryImpl> new_entry =
      new SimpleEntryImpl(index, path, key);
  SynchronousCreationCallback sync_creation_callback =
      base::Bind(&SimpleEntryImpl::CreationOperationComplete,
                 new_entry, entry, callback);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::CreateEntry, path,
                                  key, MessageLoopProxy::current(),
                                  sync_creation_callback),
                       true);
  return net::ERR_IO_PENDING;
}

// static
int SimpleEntryImpl::DoomEntry(SimpleIndex* index,
                               const FilePath& path,
                               const std::string& key,
                               const CompletionCallback& callback) {
  index->Remove(key);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::DoomEntry, path, key,
                                  MessageLoopProxy::current(), callback),
                       true);
  return net::ERR_IO_PENDING;
}

void SimpleEntryImpl::Doom() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
#if defined(OS_POSIX)
  // This call to static SimpleEntryImpl::DoomEntry() will just erase the
  // underlying files. On POSIX, this is fine; the files are still open on the
  // SimpleSynchronousEntry, and operations can even happen on them. The files
  // will be removed from the filesystem when they are closed.
  DoomEntry(index_, path_, key_, CompletionCallback());
#else
  NOTIMPLEMENTED();
#endif
}

void SimpleEntryImpl::Close() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // Postpone close operation.
  // Push the close operation to the end of the line. This way we run all
  // operations before we are able close.
  pending_operations_.push(base::Bind(&SimpleEntryImpl::CloseInternal, this));
  RunNextOperationIfNeeded();
}

std::string SimpleEntryImpl::GetKey() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return key_;
}

Time SimpleEntryImpl::GetLastUsed() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return last_used_;
}

Time SimpleEntryImpl::GetLastModified() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return last_modified_;
}

int32 SimpleEntryImpl::GetDataSize(int index) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return data_size_[index];
}

int SimpleEntryImpl::ReadData(int index,
                              int offset,
                              net::IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (index < 0 || index >= kSimpleEntryFileCount || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;
  if (offset >= data_size_[index] || offset < 0 || !buf_len)
    return 0;
  // TODO(felipeg): Optimization: Add support for truly parallel read
  // operations.
  pending_operations_.push(
      base::Bind(&SimpleEntryImpl::ReadDataInternal,
                 this,
                 index,
                 offset,
                 make_scoped_refptr(buf),
                 buf_len,
                 callback));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::WriteData(int index,
                               int offset,
                               net::IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback,
                               bool truncate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (index < 0 || index >= kSimpleEntryFileCount || offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;
  pending_operations_.push(
      base::Bind(&SimpleEntryImpl::WriteDataInternal,
                 this,
                 index,
                 offset,
                 make_scoped_refptr(buf),
                 buf_len,
                 callback,
                 truncate));
  RunNextOperationIfNeeded();
  // TODO(felipeg): Optimization: Add support for optimistic writes, quickly
  // returning net::OK here.
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::ReadSparseData(int64 offset,
                                    net::IOBuffer* buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(gavinp): Determine if the simple backend should support sparse data.
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

int SimpleEntryImpl::WriteSparseData(int64 offset,
                                     net::IOBuffer* buf,
                                     int buf_len,
                                     const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(gavinp): Determine if the simple backend should support sparse data.
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

int SimpleEntryImpl::GetAvailableRange(int64 offset,
                                       int len,
                                       int64* start,
                                       const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(gavinp): Determine if the simple backend should support sparse data.
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

bool SimpleEntryImpl::CouldBeSparse() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(gavinp): Determine if the simple backend should support sparse data.
  return false;
}

void SimpleEntryImpl::CancelSparseIO() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(gavinp): Determine if the simple backend should support sparse data.
  NOTIMPLEMENTED();
}

int SimpleEntryImpl::ReadyForSparseIO(const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(gavinp): Determine if the simple backend should support sparse data.
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

SimpleEntryImpl::SimpleEntryImpl(SimpleIndex* index,
                                 const FilePath& path,
                                 const std::string& key)
    :  index_(index->AsWeakPtr()),
       path_(path),
       key_(key),
       synchronous_entry_(NULL),
       operation_running_(false) {
}

SimpleEntryImpl::~SimpleEntryImpl() {
  DCHECK_EQ(0U, pending_operations_.size());
  DCHECK(!operation_running_);
}

bool SimpleEntryImpl::RunNextOperationIfNeeded() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (pending_operations_.size() <= 0 || operation_running_)
    return false;
  base::Closure operation = pending_operations_.front();
  pending_operations_.pop();
  operation.Run();
  return true;
}

void SimpleEntryImpl::CloseInternal() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(pending_operations_.size() == 0);
  DCHECK(!operation_running_);
  DCHECK(synchronous_entry_);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::Close,
                                  base::Unretained(synchronous_entry_)),
                       true);
  // Entry::Close() is expected to delete this entry. See disk_cache.h for
  // details.
  Release();  // Balanced in CreationOperationCompleted().
}

void SimpleEntryImpl::ReadDataInternal(int index,
                                       int offset,
                                       scoped_refptr<net::IOBuffer> buf,
                                       int buf_len,
                                       const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(!operation_running_);
  operation_running_ = true;
  if (index_)
    index_->UseIfExists(key_);
  SynchronousOperationCallback sync_operation_callback =
      base::Bind(&SimpleEntryImpl::EntryOperationComplete,
                 this, callback);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::ReadData,
                                  base::Unretained(synchronous_entry_),
                                  index, offset, buf,
                                  buf_len, sync_operation_callback),
                       true);
}

void SimpleEntryImpl::WriteDataInternal(int index,
                                       int offset,
                                       scoped_refptr<net::IOBuffer> buf,
                                       int buf_len,
                                       const CompletionCallback& callback,
                                       bool truncate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(!operation_running_);
  operation_running_ = true;
  if (index_)
    index_->UseIfExists(key_);

  // TODO(felipeg): When adding support for optimistic writes we need to set
  // data_size_[index] = buf_len here.
  SynchronousOperationCallback sync_operation_callback =
      base::Bind(&SimpleEntryImpl::EntryOperationComplete,
                 this, callback);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::WriteData,
                                  base::Unretained(synchronous_entry_),
                                  index, offset, buf,
                                  buf_len, sync_operation_callback, truncate),
                       true);
}

void SimpleEntryImpl::CreationOperationComplete(
    Entry** out_entry,
    const CompletionCallback& completion_callback,
    SimpleSynchronousEntry* sync_entry) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (!sync_entry) {
    completion_callback.Run(net::ERR_FAILED);
    // If OpenEntry failed, we must remove it from our index.
    if (index_)
      index_->Remove(key_);
    // The reference held by the Callback calling us will go out of scope and
    // delete |this| on leaving this scope.
    return;
  }
  // The Backend interface requires us to return |this|, and keep the Entry
  // alive until Entry::Close(). Adding a reference to self will keep |this|
  // alive after the scope of the Callback calling us is destroyed.
  AddRef();  // Balanced in CloseInternal().
  synchronous_entry_ = sync_entry;
  SetSynchronousData();
  if (index_)
    index_->Insert(key_);
  *out_entry = this;
  completion_callback.Run(net::OK);
}

void SimpleEntryImpl::EntryOperationComplete(
    const CompletionCallback& completion_callback,
    int result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);

  operation_running_ = false;
  SetSynchronousData();
  if (index_) {
    if (result >= 0) {
      index_->UpdateEntrySize(synchronous_entry_->key(),
                              synchronous_entry_->GetFileSize());
    } else {
      index_->Remove(synchronous_entry_->key());
    }
  }
  completion_callback.Run(result);
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::SetSynchronousData() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(!operation_running_);
  // TODO(felipeg): These copies to avoid data races are not optimal. While
  // adding an IO thread index (for fast misses etc...), we can store this data
  // in that structure. This also solves problems with last_used() on ext4
  // filesystems not being accurate.
  last_used_ = synchronous_entry_->last_used();
  last_modified_ = synchronous_entry_->last_modified();
  for (int i = 0; i < kSimpleEntryFileCount; ++i)
    data_size_[i] = synchronous_entry_->data_size(i);
}

}  // namespace disk_cache
