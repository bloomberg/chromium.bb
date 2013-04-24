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
#include "base/metrics/histogram.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "third_party/zlib/zlib.h"

namespace {

void CallCompletionCallback(const net::CompletionCallback& callback,
                            scoped_ptr<int> result) {
  DCHECK(result);
  if (!callback.is_null())
    callback.Run(*result);
}

}  // namespace

namespace disk_cache {

using base::Closure;
using base::FilePath;
using base::MessageLoopProxy;
using base::Time;
using base::WorkerPool;

// static
int SimpleEntryImpl::OpenEntry(SimpleIndex* entry_index,
                               const FilePath& path,
                               const std::string& key,
                               Entry** out_entry,
                               const CompletionCallback& callback) {
  // This enumeration is used in histograms, add entries only at end.
  enum OpenEntryIndexEnum {
    INDEX_NOEXIST = 0,
    INDEX_MISS = 1,
    INDEX_HIT = 2,
    INDEX_MAX = 3,
  };
  OpenEntryIndexEnum open_entry_index_enum = INDEX_NOEXIST;
  if (entry_index) {
    if (entry_index->Has(key))
      open_entry_index_enum = INDEX_HIT;
    else
      open_entry_index_enum = INDEX_MISS;
  }
  UMA_HISTOGRAM_ENUMERATION("SimpleCache.OpenEntryIndexState",
                            open_entry_index_enum, INDEX_MAX);
  // If entry is not known to the index, initiate fast failover to the network.
  if (open_entry_index_enum == INDEX_MISS)
    return net::ERR_FAILED;

  const base::TimeTicks start_time = base::TimeTicks::Now();
  // Go down to the disk to find the entry.
  scoped_refptr<SimpleEntryImpl> new_entry =
      new SimpleEntryImpl(entry_index, path, key);
  typedef SimpleSynchronousEntry* PointerToSimpleSynchronousEntry;
  scoped_ptr<PointerToSimpleSynchronousEntry> sync_entry(
      new PointerToSimpleSynchronousEntry());
  Closure task = base::Bind(&SimpleSynchronousEntry::OpenEntry, path, key,
                            sync_entry.get());
  Closure reply = base::Bind(&SimpleEntryImpl::CreationOperationComplete,
                             new_entry,
                             callback,
                             start_time,
                             base::Passed(&sync_entry),
                             out_entry);
  WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
  return net::ERR_IO_PENDING;
}

// static
int SimpleEntryImpl::CreateEntry(SimpleIndex* entry_index,
                                 const FilePath& path,
                                 const std::string& key,
                                 Entry** out_entry,
                                 const CompletionCallback& callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  // We insert the entry in the index before creating the entry files in the
  // SimpleSynchronousEntry, because this way the worse scenario is when we
  // have the entry in the index but we don't have the created files yet, this
  // way we never leak files. CreationOperationComplete will remove the entry
  // from the index if the creation fails.
  if (entry_index)
    entry_index->Insert(key);
  scoped_refptr<SimpleEntryImpl> new_entry =
      new SimpleEntryImpl(entry_index, path, key);

  typedef SimpleSynchronousEntry* PointerToSimpleSynchronousEntry;
  scoped_ptr<PointerToSimpleSynchronousEntry> sync_entry(
      new PointerToSimpleSynchronousEntry());
  Closure task = base::Bind(&SimpleSynchronousEntry::CreateEntry, path, key,
                            sync_entry.get());
  Closure reply = base::Bind(&SimpleEntryImpl::CreationOperationComplete,
                             new_entry,
                             callback,
                             start_time,
                             base::Passed(&sync_entry),
                             out_entry);
  WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
  return net::ERR_IO_PENDING;
}

// static
int SimpleEntryImpl::DoomEntry(SimpleIndex* entry_index,
                               const FilePath& path,
                               const std::string& key,
                               const CompletionCallback& callback) {
  entry_index->Remove(key);
  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::DoomEntry, path, key,
                            result.get());
  Closure reply = base::Bind(&CallCompletionCallback,
                             callback, base::Passed(&result));
  WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
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
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(0U, pending_operations_.size());
  DCHECK(!operation_running_);
  DCHECK(!synchronous_entry_);
}

bool SimpleEntryImpl::RunNextOperationIfNeeded() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_CUSTOM_COUNTS("SimpleCache.EntryOperationsPending",
                              pending_operations_.size(), 0, 100, 20);
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
  synchronous_entry_ = NULL;
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

  scoped_ptr<uint32> read_crc32(new uint32());
  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::ReadData,
                            base::Unretained(synchronous_entry_),
                            stream_index, offset, make_scoped_refptr(buf),
                            buf_len, read_crc32.get(), result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::ReadOperationComplete, this,
                             stream_index, offset, callback,
                             base::Passed(&read_crc32), base::Passed(&result));
  WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
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

  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::WriteData,
                            base::Unretained(synchronous_entry_),
                            stream_index, offset, make_scoped_refptr(buf),
                            buf_len, truncate, result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::EntryOperationComplete, this,
                             stream_index, callback, base::Passed(&result));
  WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
}

void SimpleEntryImpl::CreationOperationComplete(
    const CompletionCallback& completion_callback,
    const base::TimeTicks& start_time,
    scoped_ptr<SimpleSynchronousEntry*> in_sync_entry,
    Entry** out_entry) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(in_sync_entry);

  bool creation_result = !*in_sync_entry;
  UMA_HISTOGRAM_BOOLEAN("SimpleCache.EntryCreationResult", creation_result);
  if (creation_result) {
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
  synchronous_entry_ = *in_sync_entry;
  SetSynchronousData();
  *out_entry = this;
  UMA_HISTOGRAM_TIMES("SimpleCache.EntryCreationTime",
                      (base::TimeTicks::Now() - start_time));

  completion_callback.Run(net::OK);
}

void SimpleEntryImpl::EntryOperationComplete(
    int stream_index,
    const CompletionCallback& completion_callback,
    scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);
  DCHECK(result);

  operation_running_ = false;

  if (*result < 0) {
    if (entry_index_)
      entry_index_->Remove(key_);
    entry_index_.reset();
    crc32s_end_offset_[stream_index] = 0;
  }
  SetSynchronousData();
  completion_callback.Run(*result);
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::ReadOperationComplete(
    int stream_index,
    int offset,
    const CompletionCallback& completion_callback,
    scoped_ptr<uint32> read_crc32,
    scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);
  DCHECK(read_crc32);
  DCHECK(result);

  if (*result > 0 && crc32s_end_offset_[stream_index] == offset) {
    uint32 current_crc = offset == 0 ? crc32(0, Z_NULL, 0)
                                     : crc32s_[stream_index];
    crc32s_[stream_index] = crc32_combine(current_crc, *read_crc32, *result);
    crc32s_end_offset_[stream_index] += *result;
    if (!have_written_[stream_index] &&
        data_size_[stream_index] == crc32s_end_offset_[stream_index]) {
      // We have just read a file from start to finish, and so we have
      // computed a crc of the entire file. We can check it now. If a cache
      // entry has a single reader, the normal pattern is to read from start
      // to finish.

      // Other cases are possible. In the case of two readers on the same
      // entry, one reader can be behind the other. In this case we compute
      // the crc as the most advanced reader progresses, and check it for
      // both readers as they read the last byte.

      scoped_ptr<int> new_result(new int());
      Closure task = base::Bind(&SimpleSynchronousEntry::CheckEOFRecord,
                                base::Unretained(synchronous_entry_),
                                stream_index, crc32s_[stream_index],
                                new_result.get());
      Closure reply = base::Bind(&SimpleEntryImpl::ChecksumOperationComplete,
                                 this, *result, stream_index,
                                 completion_callback,
                                 base::Passed(&new_result));
      WorkerPool::PostTaskAndReply(FROM_HERE, task, reply, true);
      return;
    }
  }
  EntryOperationComplete(stream_index, completion_callback, result.Pass());
}

void SimpleEntryImpl::ChecksumOperationComplete(
    int orig_result,
    int stream_index,
    const CompletionCallback& completion_callback,
    scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK(operation_running_);
  DCHECK(result);
  if (*result == net::OK)
    *result = orig_result;
  EntryOperationComplete(stream_index, completion_callback, result.Pass());
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
