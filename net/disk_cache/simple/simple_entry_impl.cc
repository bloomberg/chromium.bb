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
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/task_runner.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/zlib/zlib.h"

namespace {

// Used in histograms, please only add entries at the end.
enum ReadResult {
  READ_RESULT_SUCCESS = 0,
  READ_RESULT_INVALID_ARGUMENT = 1,
  READ_RESULT_NONBLOCK_EMPTY_RETURN = 2,
  READ_RESULT_BAD_STATE = 3,
  READ_RESULT_FAST_EMPTY_RETURN = 4,
  READ_RESULT_SYNC_READ_FAILURE = 5,
  READ_RESULT_SYNC_CHECKSUM_FAILURE = 6,
  READ_RESULT_MAX = 7,
};

// Used in histograms, please only add entries at the end.
enum WriteResult {
  WRITE_RESULT_SUCCESS = 0,
  WRITE_RESULT_INVALID_ARGUMENT = 1,
  WRITE_RESULT_OVER_MAX_SIZE = 2,
  WRITE_RESULT_BAD_STATE = 3,
  WRITE_RESULT_SYNC_WRITE_FAILURE = 4,
  WRITE_RESULT_MAX = 5,
};

void RecordReadResult(ReadResult result) {
  UMA_HISTOGRAM_ENUMERATION("SimpleCache.ReadResult", result, READ_RESULT_MAX);
};

void RecordWriteResult(WriteResult result) {
  UMA_HISTOGRAM_ENUMERATION("SimpleCache.WriteResult",
                            result, WRITE_RESULT_MAX);
};

// Short trampoline to take an owned input parameter and call a net completion
// callback with its value.
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
using base::TaskRunner;

// A helper class to insure that RunNextOperationIfNeeded() is called when
// exiting the current stack frame.
class SimpleEntryImpl::ScopedOperationRunner {
 public:
  explicit ScopedOperationRunner(SimpleEntryImpl* entry) : entry_(entry) {
  }

  ~ScopedOperationRunner() {
    entry_->RunNextOperationIfNeeded();
  }

 private:
  SimpleEntryImpl* const entry_;
};

SimpleEntryImpl::SimpleEntryImpl(SimpleBackendImpl* backend,
                                 const FilePath& path,
                                 const std::string& key,
                                 const uint64 entry_hash)
    : backend_(backend->AsWeakPtr()),
      worker_pool_(backend->worker_pool()),
      path_(path),
      key_(key),
      entry_hash_(entry_hash),
      last_used_(Time::Now()),
      last_modified_(last_used_),
      open_count_(0),
      state_(STATE_UNINITIALIZED),
      synchronous_entry_(NULL) {
  DCHECK_EQ(entry_hash, simple_util::GetEntryHashKey(key));
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc32s_end_offset_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc32s_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(have_written_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc_check_state_),
                 arrays_should_be_same_size);
  MakeUninitialized();
}

int SimpleEntryImpl::OpenEntry(Entry** out_entry,
                               const CompletionCallback& callback) {
  DCHECK(backend_.get());
  // This enumeration is used in histograms, add entries only at end.
  enum OpenEntryIndexEnum {
    INDEX_NOEXIST = 0,
    INDEX_MISS = 1,
    INDEX_HIT = 2,
    INDEX_MAX = 3,
  };
  OpenEntryIndexEnum open_entry_index_enum = INDEX_NOEXIST;
  if (backend_.get()) {
    if (backend_->index()->Has(key_))
      open_entry_index_enum = INDEX_HIT;
    else
      open_entry_index_enum = INDEX_MISS;
  }
  UMA_HISTOGRAM_ENUMERATION("SimpleCache.OpenEntryIndexState",
                            open_entry_index_enum, INDEX_MAX);

  // If entry is not known to the index, initiate fast failover to the network.
  if (open_entry_index_enum == INDEX_MISS)
    return net::ERR_FAILED;

  pending_operations_.push(base::Bind(&SimpleEntryImpl::OpenEntryInternal,
                                      this, callback, out_entry));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::CreateEntry(Entry** out_entry,
                                 const CompletionCallback& callback) {
  DCHECK(backend_.get());
  int ret_value = net::ERR_FAILED;
  if (state_ == STATE_UNINITIALIZED &&
      pending_operations_.size() == 0) {
    ReturnEntryToCaller(out_entry);
    // We can do optimistic Create.
    pending_operations_.push(base::Bind(&SimpleEntryImpl::CreateEntryInternal,
                                        this,
                                        CompletionCallback(),
                                        static_cast<Entry**>(NULL)));
    ret_value = net::OK;
  } else {
    pending_operations_.push(base::Bind(&SimpleEntryImpl::CreateEntryInternal,
                                        this,
                                        callback,
                                        out_entry));
    ret_value = net::ERR_IO_PENDING;
  }

  // We insert the entry in the index before creating the entry files in the
  // SimpleSynchronousEntry, because this way the worst scenario is when we
  // have the entry in the index but we don't have the created files yet, this
  // way we never leak files. CreationOperationComplete will remove the entry
  // from the index if the creation fails.
  if (backend_.get())
    backend_->index()->Insert(key_);

  RunNextOperationIfNeeded();
  return ret_value;
}

int SimpleEntryImpl::DoomEntry(const CompletionCallback& callback) {
  MarkAsDoomed();
  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::DoomEntry, path_, key_,
                            entry_hash_, result.get());
  Closure reply = base::Bind(&CallCompletionCallback,
                             callback, base::Passed(&result));
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
  return net::ERR_IO_PENDING;
}


void SimpleEntryImpl::Doom() {
  DoomEntry(CompletionCallback());
}

void SimpleEntryImpl::Close() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_LT(0, open_count_);

  if (--open_count_ > 0) {
    DCHECK(!HasOneRef());
    Release();  // Balanced in ReturnEntryToCaller().
    return;
  }

  pending_operations_.push(base::Bind(&SimpleEntryImpl::CloseInternal, this));
  DCHECK(!HasOneRef());
  Release();  // Balanced in ReturnEntryToCaller().
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
  DCHECK_LE(0, data_size_[stream_index]);
  return data_size_[stream_index];
}

int SimpleEntryImpl::ReadData(int stream_index,
                              int offset,
                              net::IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (stream_index < 0 || stream_index >= kSimpleEntryFileCount ||
      buf_len < 0) {
    RecordReadResult(READ_RESULT_INVALID_ARGUMENT);
    return net::ERR_INVALID_ARGUMENT;
  }
  if (pending_operations_.empty() && (offset >= GetDataSize(stream_index) ||
                                      offset < 0 || !buf_len)) {
    RecordReadResult(READ_RESULT_NONBLOCK_EMPTY_RETURN);
    return 0;
  }

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
    RecordWriteResult(WRITE_RESULT_INVALID_ARGUMENT);
    return net::ERR_INVALID_ARGUMENT;
  }
  if (backend_.get() && offset + buf_len > backend_->GetMaxFileSize()) {
    RecordWriteResult(WRITE_RESULT_OVER_MAX_SIZE);
    return net::ERR_FAILED;
  }

  int ret_value = net::ERR_FAILED;
  if (state_ == STATE_READY && pending_operations_.size() == 0) {
    // We can only do optimistic Write if there is no pending operations, so
    // that we are sure that the next call to RunNextOperationIfNeeded will
    // actually run the write operation that sets the stream size. It also
    // prevents from previous possibly-conflicting writes that could be stacked
    // in the |pending_operations_|. We could optimize this for when we have
    // only read operations enqueued.
    // TODO(gavinp,pasko): For performance, don't use a copy of an IOBuffer
    // here to avoid paying the price of the RefCountedThreadSafe atomic
    // operations.
    IOBuffer* buf_copy = NULL;
    if (buf) {
      buf_copy = new IOBuffer(buf_len);
      memcpy(buf_copy->data(), buf->data(), buf_len);
    }
    pending_operations_.push(
        base::Bind(&SimpleEntryImpl::WriteDataInternal, this, stream_index,
                   offset, make_scoped_refptr(buf_copy), buf_len,
                   CompletionCallback(), truncate));
    ret_value = buf_len;
  } else {
    pending_operations_.push(
        base::Bind(&SimpleEntryImpl::WriteDataInternal, this, stream_index,
                   offset, make_scoped_refptr(buf), buf_len, callback,
                   truncate));
    ret_value = net::ERR_IO_PENDING;
  }

  RunNextOperationIfNeeded();
  return ret_value;
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

SimpleEntryImpl::~SimpleEntryImpl() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(0U, pending_operations_.size());
  DCHECK(state_ == STATE_UNINITIALIZED || state_ == STATE_FAILURE);
  DCHECK(!synchronous_entry_);
  RemoveSelfFromBackend();
}

void SimpleEntryImpl::MakeUninitialized() {
  state_ = STATE_UNINITIALIZED;
  std::memset(crc32s_end_offset_, 0, sizeof(crc32s_end_offset_));
  std::memset(crc32s_, 0, sizeof(crc32s_));
  std::memset(have_written_, 0, sizeof(have_written_));
  std::memset(data_size_, 0, sizeof(data_size_));
  std::memset(crc_check_state_, 0, sizeof(crc_check_state_));
}

void SimpleEntryImpl::ReturnEntryToCaller(Entry** out_entry) {
  DCHECK(out_entry);
  ++open_count_;
  AddRef();  // Balanced in Close()
  *out_entry = this;
}

void SimpleEntryImpl::RemoveSelfFromBackend() {
  if (!backend_.get())
    return;
  backend_->OnDeactivated(this);
  backend_.reset();
}

void SimpleEntryImpl::MarkAsDoomed() {
  if (!backend_.get())
    return;
  backend_->index()->Remove(key_);
  RemoveSelfFromBackend();
}

void SimpleEntryImpl::RunNextOperationIfNeeded() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_CUSTOM_COUNTS("SimpleCache.EntryOperationsPending",
                              pending_operations_.size(), 0, 100, 20);
  if (!pending_operations_.empty() && state_ != STATE_IO_PENDING) {
    base::Closure operation = pending_operations_.front();
    pending_operations_.pop();
    operation.Run();
    // |this| may have been deleted.
  }
}

void SimpleEntryImpl::OpenEntryInternal(const CompletionCallback& callback,
                                        Entry** out_entry) {
  ScopedOperationRunner operation_runner(this);
  if (state_ == STATE_READY) {
    ReturnEntryToCaller(out_entry);
    MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(callback,
                                                                net::OK));
    return;
  } else if (state_ == STATE_FAILURE) {
    if (!callback.is_null()) {
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, net::ERR_FAILED));
    }
    return;
  }
  DCHECK_EQ(STATE_UNINITIALIZED, state_);
  DCHECK(!synchronous_entry_);
  state_ = STATE_IO_PENDING;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  typedef SimpleSynchronousEntry* PointerToSimpleSynchronousEntry;
  scoped_ptr<PointerToSimpleSynchronousEntry> sync_entry(
      new PointerToSimpleSynchronousEntry());
  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::OpenEntry, path_, key_,
                            entry_hash_, sync_entry.get(), result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::CreationOperationComplete, this,
                             callback, start_time, base::Passed(&sync_entry),
                             base::Passed(&result), out_entry);
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::CreateEntryInternal(const CompletionCallback& callback,
                                          Entry** out_entry) {
  ScopedOperationRunner operation_runner(this);
  if (state_ != STATE_UNINITIALIZED) {
    // There is already an active normal entry.
    if (!callback.is_null()) {
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, net::ERR_FAILED));
    }
    return;
  }
  DCHECK_EQ(STATE_UNINITIALIZED, state_);
  DCHECK(!synchronous_entry_);

  state_ = STATE_IO_PENDING;

  // Since we don't know the correct values for |last_used_| and
  // |last_modified_| yet, we make this approximation.
  last_used_ = last_modified_ = base::Time::Now();

  // If creation succeeds, we should mark all streams to be saved on close.
  for (int i = 0; i < kSimpleEntryFileCount; ++i)
    have_written_[i] = true;

  const base::TimeTicks start_time = base::TimeTicks::Now();
  typedef SimpleSynchronousEntry* PointerToSimpleSynchronousEntry;
  scoped_ptr<PointerToSimpleSynchronousEntry> sync_entry(
      new PointerToSimpleSynchronousEntry());
  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::CreateEntry, path_, key_,
                            entry_hash_, sync_entry.get(), result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::CreationOperationComplete, this,
                             callback, start_time, base::Passed(&sync_entry),
                             base::Passed(&result), out_entry);
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::CloseInternal() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  typedef SimpleSynchronousEntry::CRCRecord CRCRecord;
  scoped_ptr<std::vector<CRCRecord> >
      crc32s_to_write(new std::vector<CRCRecord>());

  if (state_ == STATE_READY) {
    DCHECK(synchronous_entry_);
    state_ = STATE_IO_PENDING;
    for (int i = 0; i < kSimpleEntryFileCount; ++i) {
      if (have_written_[i]) {
        if (GetDataSize(i) == crc32s_end_offset_[i]) {
          int32 crc = GetDataSize(i) == 0 ? crc32(0, Z_NULL, 0) : crc32s_[i];
          crc32s_to_write->push_back(CRCRecord(i, true, crc));
        } else {
          crc32s_to_write->push_back(CRCRecord(i, false, 0));
        }
      }
    }
  } else {
    DCHECK(STATE_UNINITIALIZED == state_ || STATE_FAILURE == state_);
  }

  if (synchronous_entry_) {
    Closure task = base::Bind(&SimpleSynchronousEntry::Close,
                              base::Unretained(synchronous_entry_),
                              base::Passed(&crc32s_to_write));
    Closure reply = base::Bind(&SimpleEntryImpl::CloseOperationComplete, this);
    synchronous_entry_ = NULL;
    worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);

    for (int i = 0; i < kSimpleEntryFileCount; ++i) {
      if (!have_written_[i]) {
        UMA_HISTOGRAM_ENUMERATION("SimpleCache.CheckCRCResult",
                                  crc_check_state_[i], CRC_CHECK_MAX);
      }
    }
  } else {
    synchronous_entry_ = NULL;
    CloseOperationComplete();
  }
}

void SimpleEntryImpl::ReadDataInternal(int stream_index,
                                       int offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       const CompletionCallback& callback) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  ScopedOperationRunner operation_runner(this);

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (!callback.is_null()) {
      RecordReadResult(READ_RESULT_BAD_STATE);
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, net::ERR_FAILED));
    }
    return;
  }
  DCHECK_EQ(STATE_READY, state_);
  if (offset >= GetDataSize(stream_index) || offset < 0 || !buf_len) {
    RecordReadResult(READ_RESULT_FAST_EMPTY_RETURN);
    // If there is nothing to read, we bail out before setting state_ to
    // STATE_IO_PENDING.
    if (!callback.is_null())
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, 0));
    return;
  }
  buf_len = std::min(buf_len, GetDataSize(stream_index) - offset);

  state_ = STATE_IO_PENDING;
  if (backend_.get())
    backend_->index()->UseIfExists(key_);

  scoped_ptr<uint32> read_crc32(new uint32());
  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::ReadData,
                            base::Unretained(synchronous_entry_),
                            stream_index, offset, make_scoped_refptr(buf),
                            buf_len, read_crc32.get(), result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::ReadOperationComplete, this,
                             stream_index, offset, callback,
                             base::Passed(&read_crc32), base::Passed(&result));
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::WriteDataInternal(int stream_index,
                                       int offset,
                                       net::IOBuffer* buf,
                                       int buf_len,
                                       const CompletionCallback& callback,
                                       bool truncate) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  ScopedOperationRunner operation_runner(this);
  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    RecordWriteResult(WRITE_RESULT_BAD_STATE);
    if (!callback.is_null()) {
      // We need to posttask so that we don't go in a loop when we call the
      // callback directly.
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, net::ERR_FAILED));
    }
    // |this| may be destroyed after return here.
    return;
  }
  DCHECK_EQ(STATE_READY, state_);
  state_ = STATE_IO_PENDING;
  if (backend_.get())
    backend_->index()->UseIfExists(key_);
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

  if (truncate) {
    data_size_[stream_index] = offset + buf_len;
  } else {
    data_size_[stream_index] = std::max(offset + buf_len,
                                        GetDataSize(stream_index));
  }

  // Since we don't know the correct values for |last_used_| and
  // |last_modified_| yet, we make this approximation.
  last_used_ = last_modified_ = base::Time::Now();

  have_written_[stream_index] = true;

  scoped_ptr<int> result(new int());
  Closure task = base::Bind(&SimpleSynchronousEntry::WriteData,
                            base::Unretained(synchronous_entry_),
                            stream_index, offset, make_scoped_refptr(buf),
                            buf_len, truncate, result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::WriteOperationComplete, this,
                             stream_index, callback, base::Passed(&result));
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::CreationOperationComplete(
    const CompletionCallback& completion_callback,
    const base::TimeTicks& start_time,
    scoped_ptr<SimpleSynchronousEntry*> in_sync_entry,
    scoped_ptr<int> in_result,
    Entry** out_entry) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_IO_PENDING);
  DCHECK(in_sync_entry);
  DCHECK(in_result);
  ScopedOperationRunner operation_runner(this);
  UMA_HISTOGRAM_BOOLEAN(
      "SimpleCache.EntryCreationResult", *in_result == net::OK);
  if (*in_result != net::OK) {
    if (*in_result!= net::ERR_FILE_EXISTS)
      MarkAsDoomed();
    if (!completion_callback.is_null()) {
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          completion_callback, net::ERR_FAILED));
    }
    MakeUninitialized();
    return;
  }
  // If out_entry is NULL, it means we already called ReturnEntryToCaller from
  // the optimistic Create case.
  if (out_entry)
    ReturnEntryToCaller(out_entry);

  state_ = STATE_READY;
  synchronous_entry_ = *in_sync_entry;
  SetSynchronousData();
  UMA_HISTOGRAM_TIMES("SimpleCache.EntryCreationTime",
                      (base::TimeTicks::Now() - start_time));

  if (!completion_callback.is_null()) {
    MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
        completion_callback, net::OK));
  }
}

void SimpleEntryImpl::EntryOperationComplete(
    int stream_index,
    const CompletionCallback& completion_callback,
    scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_IO_PENDING, state_);
  DCHECK(result);
  state_ = STATE_READY;
  if (*result < 0) {
    MarkAsDoomed();
    state_ = STATE_FAILURE;
    crc32s_end_offset_[stream_index] = 0;
  } else {
    SetSynchronousData();
  }

  if (!completion_callback.is_null()) {
    MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
        completion_callback, *result));
  }
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
  DCHECK_EQ(STATE_IO_PENDING, state_);
  DCHECK(read_crc32);
  DCHECK(result);

  if (*result > 0 && crc32s_end_offset_[stream_index] == offset) {
    uint32 current_crc = offset == 0 ? crc32(0, Z_NULL, 0)
                                     : crc32s_[stream_index];
    crc32s_[stream_index] = crc32_combine(current_crc, *read_crc32, *result);
    crc32s_end_offset_[stream_index] += *result;
    if (!have_written_[stream_index] &&
        GetDataSize(stream_index) == crc32s_end_offset_[stream_index]) {
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
      worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
      crc_check_state_[stream_index] = CRC_CHECK_DONE;
      return;
    }
  }
  if (*result < 0) {
    RecordReadResult(READ_RESULT_SYNC_READ_FAILURE);
  } else {
    RecordReadResult(READ_RESULT_SUCCESS);
    if (crc_check_state_[stream_index] == CRC_CHECK_NEVER_READ_TO_END &&
        offset + *result == GetDataSize(stream_index)) {
      crc_check_state_[stream_index] = CRC_CHECK_NOT_DONE;
    }
  }
  EntryOperationComplete(stream_index, completion_callback, result.Pass());
}

void SimpleEntryImpl::WriteOperationComplete(
    int stream_index,
    const CompletionCallback& completion_callback,
    scoped_ptr<int> result) {
  if (*result >= 0)
    RecordWriteResult(WRITE_RESULT_SUCCESS);
  else
    RecordWriteResult(WRITE_RESULT_SYNC_WRITE_FAILURE);
  EntryOperationComplete(stream_index, completion_callback, result.Pass());
}

void SimpleEntryImpl::ChecksumOperationComplete(
    int orig_result,
    int stream_index,
    const CompletionCallback& completion_callback,
    scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_IO_PENDING, state_);
  DCHECK(result);
  if (*result == net::OK) {
    *result = orig_result;
    if (orig_result >= 0)
      RecordReadResult(READ_RESULT_SUCCESS);
    else
      RecordReadResult(READ_RESULT_SYNC_READ_FAILURE);
  } else {
    RecordReadResult(READ_RESULT_SYNC_CHECKSUM_FAILURE);
  }
  EntryOperationComplete(stream_index, completion_callback, result.Pass());
}

void SimpleEntryImpl::CloseOperationComplete() {
  DCHECK(!synchronous_entry_);
  DCHECK_EQ(0, open_count_);
  DCHECK(STATE_IO_PENDING == state_ || STATE_FAILURE == state_ ||
         STATE_UNINITIALIZED == state_);
  MakeUninitialized();
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::SetSynchronousData() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_READY, state_);
  // TODO(felipeg): These copies to avoid data races are not optimal. While
  // adding an IO thread index (for fast misses etc...), we can store this data
  // in that structure. This also solves problems with last_used() on ext4
  // filesystems not being accurate.
  last_used_ = synchronous_entry_->last_used();
  last_modified_ = synchronous_entry_->last_modified();
  for (int i = 0; i < kSimpleEntryFileCount; ++i)
    data_size_[i] = synchronous_entry_->data_size(i);
  if (backend_.get())
    backend_->index()->UpdateEntrySize(key_, synchronous_entry_->GetFileSize());
}

}  // namespace disk_cache
