// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_entry_impl.h"

#include <algorithm>
#include <cstring>
#include <vector>

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
#include "third_party/zlib/zlib.h"

namespace {

typedef disk_cache::Entry::CompletionCallback CompletionCallback;
typedef disk_cache::SimpleSynchronousEntry::SynchronousCreationCallback
    SynchronousCreationCallback;
typedef disk_cache::SimpleSynchronousEntry::SynchronousReadCallback
    SynchronousReadCallback;
typedef disk_cache::SimpleSynchronousEntry::SynchronousOperationCallback
    SynchronousOperationCallback;

}  // namespace

namespace disk_cache {

using base::FilePath;
using base::MessageLoopProxy;
using base::Time;
using base::WorkerPool;

// static
int SimpleEntryImpl::OpenEntry(SimpleIndex* entry_index,
                               const FilePath& path,
                               const std::string& key,
                               Entry** entry,
                               const CompletionCallback& callback) {
  // If entry is not known to the index, initiate fast failover to the network.
  if (entry_index && !entry_index->Has(key))
    return net::ERR_FAILED;

  scoped_refptr<SimpleEntryImpl> new_entry =
      new SimpleEntryImpl(entry_index, path, key);
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

// static
int SimpleEntryImpl::CreateEntry(SimpleIndex* entry_index,
                                 const FilePath& path,
                                 const std::string& key,
                                 Entry** entry,
                                 const CompletionCallback& callback) {
  // We insert the entry in the index before creating the entry files in the
  // SimpleSynchronousEntry, because this way the worse scenario is when we
  // have the entry in the index but we don't have the created files yet, this
  // way we never leak files. CreationOperationComplete will remove the entry
  // from the index if the creation fails.
  if (entry_index)
    entry_index->Insert(key);
  scoped_refptr<SimpleEntryImpl> new_entry =
      new SimpleEntryImpl(entry_index, path, key);
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
int SimpleEntryImpl::DoomEntry(SimpleIndex* entry_index,
                               const FilePath& path,
                               const std::string& key,
                               const CompletionCallback& callback) {
  entry_index->Remove(key);
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
  DoomEntry(entry_index_, path_, key_, CompletionCallback());
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

int32 SimpleEntryImpl::GetDataSize(int stream_index) const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return data_size_[stream_index];
}

int SimpleEntryImpl::ReadData(int stream_index,
                              int offset,
                              net::IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (stream_index < 0 || stream_index >= kSimpleEntryFileCount || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;
  if (offset >= data_size_[stream_index] || offset < 0 || !buf_len)
    return 0;
  buf_len = std::min(buf_len, data_size_[stream_index] - offset);
  // TODO(felipeg): Optimization: Add support for truly parallel read
  // operations.
  pending_operations_.push(
      base::Bind(&SimpleEntryImpl::ReadDataInternal,
                 this,
                 stream_index,
                 offset,
                 make_scoped_refptr(buf),
                 buf_len,
                 callback));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::WriteData(int stream_index,
                               int offset,
                               net::IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback,
                               bool truncate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (stream_index < 0 || stream_index >= kSimpleEntryFileCount || offset < 0 ||
      buf_len < 0) {
    return net::ERR_INVALID_ARGUMENT;
  }
  pending_operations_.push(
      base::Bind(&SimpleEntryImpl::WriteDataInternal,
                 this,
                 stream_index,
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

SimpleEntryImpl::SimpleEntryImpl(SimpleIndex* entry_index,
                                 const FilePath& path,
                                 const std::string& key)
    :  entry_index_(entry_index->AsWeakPtr()),
       path_(path),
       key_(key),
       synchronous_entry_(NULL),
       operation_running_(false) {
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc32s_end_offset_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc32s_),
                 arrays_should_be_same_size2);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(have_written_),
                 arrays_should_be_same_size3);

  std::memset(crc32s_end_offset_, 0, sizeof(crc32s_end_offset_));
  std::memset(crc32s_, 0, sizeof(crc32s_));
  std::memset(have_written_, 0, sizeof(have_written_));
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
  DCHECK_EQ(0U, pending_operations_.size());
  DCHECK(!operation_running_);
  DCHECK(synchronous_entry_);

  typedef SimpleSynchronousEntry::CRCRecord CRCRecord;

  scoped_ptr<std::vector<CRCRecord> >
      crc32s_to_write(new std::vector<CRCRecord>());
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    if (have_written_[i]) {
      if (data_size_[i] == crc32s_end_offset_[i])
        crc32s_to_write->push_back(CRCRecord(i, true, crc32s_[i]));
      else
        crc32s_to_write->push_back(CRCRecord(i, false, 0));
    }
  }
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::Close,
                                  base::Unretained(synchronous_entry_),
                                  base::Passed(&crc32s_to_write)),
                       true);
  // Entry::Close() is expected to delete this entry. See disk_cache.h for
  // details.
  Release();  // Balanced in CreationOperationComplete().
}

void SimpleEntryImpl::ReadDataInternal(int stream_index,
                                       int offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(!operation_running_);
  operation_running_ = true;
  if (entry_index_)
      entry_index_->UseIfExists(key_);
  SynchronousReadCallback sync_read_callback =
      base::Bind(&SimpleEntryImpl::ReadOperationComplete,
                 this, stream_index, offset, callback);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::ReadData,
                                  base::Unretained(synchronous_entry_),
                                  stream_index, offset, make_scoped_refptr(buf),
                                  buf_len, sync_read_callback),
                       true);
}

void SimpleEntryImpl::WriteDataInternal(int stream_index,
                                       int offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       const CompletionCallback& callback,
                                       bool truncate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(!operation_running_);
  operation_running_ = true;
  if (entry_index_)
    entry_index_->UseIfExists(key_);
  // It is easy to incrementally compute the CRC from [0 .. |offset + buf_len|)
  // if |offset == 0| or we have already computed the CRC for [0 .. offset).
  // We rely on most write operations being sequential, start to end to compute
  // the crc of the data. When we write to an entry and close without having
  // done a sequential write, we don't check the CRC on read.
  if (offset == 0 || crc32s_end_offset_[stream_index] == offset) {
    uint32 initial_crc = (offset != 0) ? crc32s_[stream_index]
                                       : crc32(0, Z_NULL, 0);
    if (buf_len > 0) {
      crc32s_[stream_index] = crc32(initial_crc,
                                    reinterpret_cast<const Bytef*>(buf->data()),
                                    buf_len);
    }
    crc32s_end_offset_[stream_index] = offset + buf_len;
  }

  have_written_[stream_index] = true;

  SynchronousOperationCallback sync_operation_callback =
      base::Bind(&SimpleEntryImpl::EntryOperationComplete,
                 this, callback, stream_index);
  WorkerPool::PostTask(FROM_HERE,
                       base::Bind(&SimpleSynchronousEntry::WriteData,
                                  base::Unretained(synchronous_entry_),
                                  stream_index, offset, make_scoped_refptr(buf),
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
    if (entry_index_)
      entry_index_->Remove(key_);
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
  *out_entry = this;
  completion_callback.Run(net::OK);
}

void SimpleEntryImpl::EntryOperationComplete(
    const CompletionCallback& completion_callback,
    int stream_index,
    int result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);

  operation_running_ = false;

  if (result < 0) {
    if (entry_index_)
      entry_index_->Remove(key_);
    entry_index_.reset();
    crc32s_end_offset_[stream_index] = 0;
  }
  SetSynchronousData();
  completion_callback.Run(result);
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::ReadOperationComplete(
    int stream_index,
    int offset,
    const CompletionCallback& completion_callback,
    int result,
    uint32 crc) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);

  if (result > 0 && crc32s_end_offset_[stream_index] == offset) {
    uint32 current_crc = offset == 0 ? crc32(0, Z_NULL, 0)
                                     : crc32s_[stream_index];
    crc32s_[stream_index] = crc32_combine(current_crc, crc, result);
    crc32s_end_offset_[stream_index] += result;
    if (!have_written_[stream_index]) {
      if (data_size_[stream_index] == crc32s_end_offset_[stream_index]) {
        // We have just read a file from start to finish, and so we have
        // computed a crc of the entire file. We can check it now. If a cache
        // entry has a single reader, the normal pattern is to read from start
        // to finish.

        // Other cases are possible. In the case of two readers on the same
        // entry, one reader can be behind the other. In this case we compute
        // the crc as the most advanced reader progresses, and check it for
        // both readers as they read the last byte.

        SynchronousOperationCallback sync_operation_callback =
            base::Bind(&SimpleEntryImpl::ChecksumOperationComplete,
                       this, result, stream_index, completion_callback);
        WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&SimpleSynchronousEntry::CheckEOFRecord,
                                        base::Unretained(synchronous_entry_),
                                        stream_index, crc32s_[stream_index],
                                        sync_operation_callback),
                             true);
        return;
      }
    }
  }
  EntryOperationComplete(completion_callback, stream_index, result);
}

void SimpleEntryImpl::ChecksumOperationComplete(
    int orig_result,
    int stream_index,
    const CompletionCallback& completion_callback,
    int result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);
  if (result == net::OK)
    result = orig_result;
  EntryOperationComplete(completion_callback, stream_index, result);
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
  if (entry_index_)
    entry_index_->UpdateEntrySize(key_, synchronous_entry_->GetFileSize());
}

}  // namespace disk_cache
