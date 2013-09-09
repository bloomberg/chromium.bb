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
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/net_log_parameters.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_net_log_parameters.h"
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

// Used in histograms, please only add entries at the end.
enum HeaderSizeChange {
  HEADER_SIZE_CHANGE_INITIAL,
  HEADER_SIZE_CHANGE_SAME,
  HEADER_SIZE_CHANGE_INCREASE,
  HEADER_SIZE_CHANGE_DECREASE,
  HEADER_SIZE_CHANGE_UNEXPECTED_WRITE,
  HEADER_SIZE_CHANGE_MAX
};

void RecordReadResult(net::CacheType cache_type, ReadResult result) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "ReadResult", cache_type, result, READ_RESULT_MAX);
}

void RecordWriteResult(net::CacheType cache_type, WriteResult result) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "WriteResult", cache_type, result, WRITE_RESULT_MAX);
}

// TODO(ttuttle): Consider removing this once we have a good handle on header
// size changes.
void RecordHeaderSizeChange(net::CacheType cache_type,
                            int old_size, int new_size) {
  HeaderSizeChange size_change;

  SIMPLE_CACHE_UMA(COUNTS_10000, "HeaderSize", cache_type, new_size);

  if (old_size == 0) {
    size_change = HEADER_SIZE_CHANGE_INITIAL;
  } else if (new_size == old_size) {
    size_change = HEADER_SIZE_CHANGE_SAME;
  } else if (new_size > old_size) {
    int delta = new_size - old_size;
    SIMPLE_CACHE_UMA(COUNTS_10000,
                     "HeaderSizeIncreaseAbsolute", cache_type, delta);
    SIMPLE_CACHE_UMA(PERCENTAGE,
                     "HeaderSizeIncreasePercentage", cache_type,
                     delta * 100 / old_size);
    size_change = HEADER_SIZE_CHANGE_INCREASE;
  } else {  // new_size < old_size
    int delta = old_size - new_size;
    SIMPLE_CACHE_UMA(COUNTS_10000,
                     "HeaderSizeDecreaseAbsolute", cache_type, delta);
    SIMPLE_CACHE_UMA(PERCENTAGE,
                     "HeaderSizeDecreasePercentage", cache_type,
                     delta * 100 / old_size);
    size_change = HEADER_SIZE_CHANGE_DECREASE;
  }

  SIMPLE_CACHE_UMA(ENUMERATION,
                   "HeaderSizeChange", cache_type,
                   size_change, HEADER_SIZE_CHANGE_MAX);
}

void RecordUnexpectedStream0Write(net::CacheType cache_type) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "HeaderSizeChange", cache_type,
                   HEADER_SIZE_CHANGE_UNEXPECTED_WRITE, HEADER_SIZE_CHANGE_MAX);
}

// Short trampoline to take an owned input parameter and call a net completion
// callback with its value.
void CallCompletionCallback(const net::CompletionCallback& callback,
                            scoped_ptr<int> result) {
  DCHECK(result);
  if (!callback.is_null())
    callback.Run(*result);
}

int g_open_entry_count = 0;

void AdjustOpenEntryCountBy(net::CacheType cache_type, int offset) {
  g_open_entry_count += offset;
  SIMPLE_CACHE_UMA(COUNTS_10000,
                   "GlobalOpenEntryCount", cache_type, g_open_entry_count);
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

SimpleEntryImpl::SimpleEntryImpl(net::CacheType cache_type,
                                 const FilePath& path,
                                 const uint64 entry_hash,
                                 OperationsMode operations_mode,
                                 SimpleBackendImpl* backend,
                                 net::NetLog* net_log)
    : backend_(backend->AsWeakPtr()),
      cache_type_(cache_type),
      worker_pool_(backend->worker_pool()),
      path_(path),
      entry_hash_(entry_hash),
      use_optimistic_operations_(operations_mode == OPTIMISTIC_OPERATIONS),
      last_used_(Time::Now()),
      last_modified_(last_used_),
      open_count_(0),
      state_(STATE_UNINITIALIZED),
      synchronous_entry_(NULL),
      net_log_(net::BoundNetLog::Make(
          net_log, net::NetLog::SOURCE_DISK_CACHE_ENTRY)) {
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc32s_end_offset_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc32s_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(have_written_),
                 arrays_should_be_same_size);
  COMPILE_ASSERT(arraysize(data_size_) == arraysize(crc_check_state_),
                 arrays_should_be_same_size);
  MakeUninitialized();
  net_log_.BeginEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY,
      CreateNetLogSimpleEntryConstructionCallback(this));
}

int SimpleEntryImpl::OpenEntry(Entry** out_entry,
                               const CompletionCallback& callback) {
  DCHECK(backend_.get());

  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_OPEN_CALL);

  bool have_index = backend_->index()->initialized();
  // This enumeration is used in histograms, add entries only at end.
  enum OpenEntryIndexEnum {
    INDEX_NOEXIST = 0,
    INDEX_MISS = 1,
    INDEX_HIT = 2,
    INDEX_MAX = 3,
  };
  OpenEntryIndexEnum open_entry_index_enum = INDEX_NOEXIST;
  if (have_index) {
    if (backend_->index()->Has(entry_hash_))
      open_entry_index_enum = INDEX_HIT;
    else
      open_entry_index_enum = INDEX_MISS;
  }
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "OpenEntryIndexState", cache_type_,
                   open_entry_index_enum, INDEX_MAX);

  // If entry is not known to the index, initiate fast failover to the network.
  if (open_entry_index_enum == INDEX_MISS) {
    net_log_.AddEventWithNetErrorCode(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_OPEN_END,
        net::ERR_FAILED);
    return net::ERR_FAILED;
  }

  pending_operations_.push(SimpleEntryOperation::OpenOperation(
      this, have_index, callback, out_entry));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

int SimpleEntryImpl::CreateEntry(Entry** out_entry,
                                 const CompletionCallback& callback) {
  DCHECK(backend_.get());
  DCHECK_EQ(entry_hash_, simple_util::GetEntryHashKey(key_));

  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CREATE_CALL);

  bool have_index = backend_->index()->initialized();
  int ret_value = net::ERR_FAILED;
  if (use_optimistic_operations_ &&
      state_ == STATE_UNINITIALIZED && pending_operations_.size() == 0) {
    net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CREATE_OPTIMISTIC);

    ReturnEntryToCaller(out_entry);
    pending_operations_.push(SimpleEntryOperation::CreateOperation(
        this, have_index, CompletionCallback(), static_cast<Entry**>(NULL)));
    ret_value = net::OK;
  } else {
    pending_operations_.push(SimpleEntryOperation::CreateOperation(
        this, have_index, callback, out_entry));
    ret_value = net::ERR_IO_PENDING;
  }

  // We insert the entry in the index before creating the entry files in the
  // SimpleSynchronousEntry, because this way the worst scenario is when we
  // have the entry in the index but we don't have the created files yet, this
  // way we never leak files. CreationOperationComplete will remove the entry
  // from the index if the creation fails.
  backend_->index()->Insert(entry_hash_);

  RunNextOperationIfNeeded();
  return ret_value;
}

int SimpleEntryImpl::DoomEntry(const CompletionCallback& callback) {
  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_DOOM_CALL);
  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_DOOM_BEGIN);

  MarkAsDoomed();
  pending_operations_.push(SimpleEntryOperation::DoomOperation(this, callback));
  RunNextOperationIfNeeded();
  return net::ERR_IO_PENDING;
}

void SimpleEntryImpl::SetKey(const std::string& key) {
  key_ = key;
  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_SET_KEY,
      net::NetLog::StringCallback("key", &key));
}

void SimpleEntryImpl::Doom() {
  DoomEntry(CompletionCallback());
}

void SimpleEntryImpl::Close() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_LT(0, open_count_);

  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CLOSE_CALL);

  if (--open_count_ > 0) {
    DCHECK(!HasOneRef());
    Release();  // Balanced in ReturnEntryToCaller().
    return;
  }

  pending_operations_.push(SimpleEntryOperation::CloseOperation(this));
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

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_CALL,
        CreateNetLogReadWriteDataCallback(stream_index, offset, buf_len,
                                          false));
  }

  if (stream_index < 0 || stream_index >= kSimpleEntryFileCount ||
      buf_len < 0) {
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_END,
          CreateNetLogReadWriteCompleteCallback(net::ERR_INVALID_ARGUMENT));
    }

    RecordReadResult(cache_type_, READ_RESULT_INVALID_ARGUMENT);
    return net::ERR_INVALID_ARGUMENT;
  }
  if (pending_operations_.empty() && (offset >= GetDataSize(stream_index) ||
                                      offset < 0 || !buf_len)) {
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_END,
          CreateNetLogReadWriteCompleteCallback(0));
    }

    RecordReadResult(cache_type_, READ_RESULT_NONBLOCK_EMPTY_RETURN);
    return 0;
  }

  // TODO(felipeg): Optimization: Add support for truly parallel read
  // operations.
  bool alone_in_queue =
      pending_operations_.size() == 0 && state_ == STATE_READY;
  pending_operations_.push(SimpleEntryOperation::ReadOperation(
      this, stream_index, offset, buf_len, buf, callback, alone_in_queue));
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

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_CALL,
        CreateNetLogReadWriteDataCallback(stream_index, offset, buf_len,
                                          truncate));
  }

  if (stream_index < 0 || stream_index >= kSimpleEntryFileCount || offset < 0 ||
      buf_len < 0) {
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(
          net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_END,
          CreateNetLogReadWriteCompleteCallback(net::ERR_INVALID_ARGUMENT));
    }
    RecordWriteResult(cache_type_, WRITE_RESULT_INVALID_ARGUMENT);
    return net::ERR_INVALID_ARGUMENT;
  }
  if (backend_.get() && offset + buf_len > backend_->GetMaxFileSize()) {
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(
          net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_END,
          CreateNetLogReadWriteCompleteCallback(net::ERR_FAILED));
    }
    RecordWriteResult(cache_type_, WRITE_RESULT_OVER_MAX_SIZE);
    return net::ERR_FAILED;
  }
  ScopedOperationRunner operation_runner(this);

  // Currently, Simple Cache is only used for HTTP, which stores the headers in
  // stream 0 and always writes them with a single, truncating write.  Detect
  // these writes and record the size and size changes of the headers.  Also,
  // note writes to stream 0 that violate those assumptions.
  if (stream_index == 0) {
    if (offset == 0 && truncate)
      RecordHeaderSizeChange(cache_type_, data_size_[0], buf_len);
    else
      RecordUnexpectedStream0Write(cache_type_);
  }

  // We can only do optimistic Write if there is no pending operations, so
  // that we are sure that the next call to RunNextOperationIfNeeded will
  // actually run the write operation that sets the stream size. It also
  // prevents from previous possibly-conflicting writes that could be stacked
  // in the |pending_operations_|. We could optimize this for when we have
  // only read operations enqueued.
  const bool optimistic =
      (use_optimistic_operations_ && state_ == STATE_READY &&
       pending_operations_.size() == 0);
  CompletionCallback op_callback;
  scoped_refptr<net::IOBuffer> op_buf;
  int ret_value = net::ERR_FAILED;
  if (!optimistic) {
    op_buf = buf;
    op_callback = callback;
    ret_value = net::ERR_IO_PENDING;
  } else {
    // TODO(gavinp,pasko): For performance, don't use a copy of an IOBuffer
    // here to avoid paying the price of the RefCountedThreadSafe atomic
    // operations.
    if (buf) {
      op_buf = new IOBuffer(buf_len);
      memcpy(op_buf->data(), buf->data(), buf_len);
    }
    op_callback = CompletionCallback();
    ret_value = buf_len;
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(
          net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_OPTIMISTIC,
          CreateNetLogReadWriteCompleteCallback(buf_len));
    }
  }

  pending_operations_.push(SimpleEntryOperation::WriteOperation(this,
                                                                stream_index,
                                                                offset,
                                                                buf_len,
                                                                op_buf.get(),
                                                                truncate,
                                                                optimistic,
                                                                op_callback));
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
  return net::ERR_NOT_IMPLEMENTED;
}

SimpleEntryImpl::~SimpleEntryImpl() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(0U, pending_operations_.size());
  DCHECK(state_ == STATE_UNINITIALIZED || state_ == STATE_FAILURE);
  DCHECK(!synchronous_entry_);
  RemoveSelfFromBackend();
  net_log_.EndEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY);
}

void SimpleEntryImpl::MakeUninitialized() {
  state_ = STATE_UNINITIALIZED;
  std::memset(crc32s_end_offset_, 0, sizeof(crc32s_end_offset_));
  std::memset(crc32s_, 0, sizeof(crc32s_));
  std::memset(have_written_, 0, sizeof(have_written_));
  std::memset(data_size_, 0, sizeof(data_size_));
  for (size_t i = 0; i < arraysize(crc_check_state_); ++i) {
    crc_check_state_[i] = CRC_CHECK_NEVER_READ_AT_ALL;
  }
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
  backend_->index()->Remove(entry_hash_);
  RemoveSelfFromBackend();
}

void SimpleEntryImpl::RunNextOperationIfNeeded() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  SIMPLE_CACHE_UMA(CUSTOM_COUNTS,
                   "EntryOperationsPending", cache_type_,
                   pending_operations_.size(), 0, 100, 20);
  if (!pending_operations_.empty() && state_ != STATE_IO_PENDING) {
    scoped_ptr<SimpleEntryOperation> operation(
        new SimpleEntryOperation(pending_operations_.front()));
    pending_operations_.pop();
    switch (operation->type()) {
      case SimpleEntryOperation::TYPE_OPEN:
        OpenEntryInternal(operation->have_index(),
                          operation->callback(),
                          operation->out_entry());
        break;
      case SimpleEntryOperation::TYPE_CREATE:
        CreateEntryInternal(operation->have_index(),
                            operation->callback(),
                            operation->out_entry());
        break;
      case SimpleEntryOperation::TYPE_CLOSE:
        CloseInternal();
        break;
      case SimpleEntryOperation::TYPE_READ:
        RecordReadIsParallelizable(*operation);
        ReadDataInternal(operation->index(),
                         operation->offset(),
                         operation->buf(),
                         operation->length(),
                         operation->callback());
        break;
      case SimpleEntryOperation::TYPE_WRITE:
        RecordWriteDependencyType(*operation);
        WriteDataInternal(operation->index(),
                          operation->offset(),
                          operation->buf(),
                          operation->length(),
                          operation->callback(),
                          operation->truncate());
        break;
      case SimpleEntryOperation::TYPE_DOOM:
        DoomEntryInternal(operation->callback());
        break;
      default:
        NOTREACHED();
    }
    // The operation is kept for histograms. Makes sure it does not leak
    // resources.
    executing_operation_.swap(operation);
    executing_operation_->ReleaseReferences();
    // |this| may have been deleted.
  }
}

void SimpleEntryImpl::OpenEntryInternal(bool have_index,
                                        const CompletionCallback& callback,
                                        Entry** out_entry) {
  ScopedOperationRunner operation_runner(this);

  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_OPEN_BEGIN);

  if (state_ == STATE_READY) {
    ReturnEntryToCaller(out_entry);
    MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(callback,
                                                                net::OK));
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_OPEN_END,
        CreateNetLogSimpleEntryCreationCallback(this, net::OK));
    return;
  }
  if (state_ == STATE_FAILURE) {
    if (!callback.is_null()) {
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, net::ERR_FAILED));
    }
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_OPEN_END,
        CreateNetLogSimpleEntryCreationCallback(this, net::ERR_FAILED));
    return;
  }

  DCHECK_EQ(STATE_UNINITIALIZED, state_);
  DCHECK(!synchronous_entry_);
  state_ = STATE_IO_PENDING;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  scoped_ptr<SimpleEntryCreationResults> results(
      new SimpleEntryCreationResults(
          SimpleEntryStat(last_used_, last_modified_, data_size_)));
  Closure task = base::Bind(&SimpleSynchronousEntry::OpenEntry,
                            cache_type_,
                            path_,
                            entry_hash_,
                            have_index,
                            results.get());
  Closure reply = base::Bind(&SimpleEntryImpl::CreationOperationComplete,
                             this,
                             callback,
                             start_time,
                             base::Passed(&results),
                             out_entry,
                             net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_OPEN_END);
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::CreateEntryInternal(bool have_index,
                                          const CompletionCallback& callback,
                                          Entry** out_entry) {
  ScopedOperationRunner operation_runner(this);

  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CREATE_BEGIN);

  if (state_ != STATE_UNINITIALIZED) {
    // There is already an active normal entry.
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CREATE_END,
        CreateNetLogSimpleEntryCreationCallback(this, net::ERR_FAILED));

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
  scoped_ptr<SimpleEntryCreationResults> results(
      new SimpleEntryCreationResults(
          SimpleEntryStat(last_used_, last_modified_, data_size_)));
  Closure task = base::Bind(&SimpleSynchronousEntry::CreateEntry,
                            cache_type_,
                            path_,
                            key_,
                            entry_hash_,
                            have_index,
                            results.get());
  Closure reply = base::Bind(&SimpleEntryImpl::CreationOperationComplete,
                             this,
                             callback,
                             start_time,
                             base::Passed(&results),
                             out_entry,
                             net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CREATE_END);
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::CloseInternal() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  typedef SimpleSynchronousEntry::CRCRecord CRCRecord;
  scoped_ptr<std::vector<CRCRecord> >
      crc32s_to_write(new std::vector<CRCRecord>());

  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CLOSE_BEGIN);

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
    Closure task =
        base::Bind(&SimpleSynchronousEntry::Close,
                   base::Unretained(synchronous_entry_),
                   SimpleEntryStat(last_used_, last_modified_, data_size_),
                   base::Passed(&crc32s_to_write));
    Closure reply = base::Bind(&SimpleEntryImpl::CloseOperationComplete, this);
    synchronous_entry_ = NULL;
    worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);

    for (int i = 0; i < kSimpleEntryFileCount; ++i) {
      if (!have_written_[i]) {
        SIMPLE_CACHE_UMA(ENUMERATION,
                         "CheckCRCResult", cache_type_,
                         crc_check_state_[i], CRC_CHECK_MAX);
      }
    }
  } else {
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

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_BEGIN,
        CreateNetLogReadWriteDataCallback(stream_index, offset, buf_len,
                                          false));
  }

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    if (!callback.is_null()) {
      RecordReadResult(cache_type_, READ_RESULT_BAD_STATE);
      MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
          callback, net::ERR_FAILED));
    }
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(
          net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_END,
          CreateNetLogReadWriteCompleteCallback(net::ERR_FAILED));
    }
    return;
  }
  DCHECK_EQ(STATE_READY, state_);
  if (offset >= GetDataSize(stream_index) || offset < 0 || !buf_len) {
    RecordReadResult(cache_type_, READ_RESULT_FAST_EMPTY_RETURN);
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
    backend_->index()->UseIfExists(entry_hash_);

  scoped_ptr<uint32> read_crc32(new uint32());
  scoped_ptr<int> result(new int());
  scoped_ptr<base::Time> last_used(new base::Time());
  Closure task = base::Bind(
      &SimpleSynchronousEntry::ReadData,
      base::Unretained(synchronous_entry_),
      SimpleSynchronousEntry::EntryOperationData(stream_index, offset, buf_len),
      make_scoped_refptr(buf),
      read_crc32.get(),
      last_used.get(),
      result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::ReadOperationComplete,
                             this,
                             stream_index,
                             offset,
                             callback,
                             base::Passed(&read_crc32),
                             base::Passed(&last_used),
                             base::Passed(&result));
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

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_BEGIN,
        CreateNetLogReadWriteDataCallback(stream_index, offset, buf_len,
                                          truncate));
  }

  if (state_ == STATE_FAILURE || state_ == STATE_UNINITIALIZED) {
    RecordWriteResult(cache_type_, WRITE_RESULT_BAD_STATE);
    if (net_log_.IsLoggingAllEvents()) {
      net_log_.AddEvent(
          net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_END,
          CreateNetLogReadWriteCompleteCallback(net::ERR_FAILED));
    }
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
    backend_->index()->UseIfExists(entry_hash_);
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

  // |entry_stat| needs to be initialized before modifying |data_size_|.
  scoped_ptr<SimpleEntryStat> entry_stat(
      new SimpleEntryStat(last_used_, last_modified_, data_size_));
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
                            SimpleSynchronousEntry::EntryOperationData(
                                stream_index, offset, buf_len, truncate),
                            make_scoped_refptr(buf),
                            entry_stat.get(),
                            result.get());
  Closure reply = base::Bind(&SimpleEntryImpl::WriteOperationComplete,
                             this,
                             stream_index,
                             callback,
                             base::Passed(&entry_stat),
                             base::Passed(&result));
  worker_pool_->PostTaskAndReply(FROM_HERE, task, reply);
}

void SimpleEntryImpl::DoomEntryInternal(const CompletionCallback& callback) {
  PostTaskAndReplyWithResult(
      worker_pool_, FROM_HERE,
      base::Bind(&SimpleSynchronousEntry::DoomEntry, path_, key_, entry_hash_),
      base::Bind(&SimpleEntryImpl::DoomOperationComplete, this, callback,
                 state_));
  state_ = STATE_IO_PENDING;
}

void SimpleEntryImpl::CreationOperationComplete(
    const CompletionCallback& completion_callback,
    const base::TimeTicks& start_time,
    scoped_ptr<SimpleEntryCreationResults> in_results,
    Entry** out_entry,
    net::NetLog::EventType end_event_type) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_IO_PENDING);
  DCHECK(in_results);
  ScopedOperationRunner operation_runner(this);
  SIMPLE_CACHE_UMA(BOOLEAN,
                   "EntryCreationResult", cache_type_,
                   in_results->result == net::OK);
  if (in_results->result != net::OK) {
    if (in_results->result != net::ERR_FILE_EXISTS)
      MarkAsDoomed();

    net_log_.AddEventWithNetErrorCode(end_event_type, net::ERR_FAILED);

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
  synchronous_entry_ = in_results->sync_entry;
  if (key_.empty()) {
    SetKey(synchronous_entry_->key());
  } else {
    // This should only be triggered when creating an entry. The key check in
    // the open case is handled in SimpleBackendImpl.
    DCHECK_EQ(key_, synchronous_entry_->key());
  }
  UpdateDataFromEntryStat(in_results->entry_stat);
  SIMPLE_CACHE_UMA(TIMES,
                   "EntryCreationTime", cache_type_,
                   (base::TimeTicks::Now() - start_time));
  AdjustOpenEntryCountBy(cache_type_, 1);

  net_log_.AddEvent(end_event_type);
  if (!completion_callback.is_null()) {
    MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(
        completion_callback, net::OK));
  }
}

void SimpleEntryImpl::EntryOperationComplete(
    int stream_index,
    const CompletionCallback& completion_callback,
    const SimpleEntryStat& entry_stat,
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
    UpdateDataFromEntryStat(entry_stat);
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
    scoped_ptr<base::Time> last_used,
    scoped_ptr<int> result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_IO_PENDING, state_);
  DCHECK(read_crc32);
  DCHECK(result);

  if (*result > 0 &&
      crc_check_state_[stream_index] == CRC_CHECK_NEVER_READ_AT_ALL) {
    crc_check_state_[stream_index] = CRC_CHECK_NEVER_READ_TO_END;
  }

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

      net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CHECKSUM_BEGIN);

      scoped_ptr<int> new_result(new int());
      Closure task = base::Bind(&SimpleSynchronousEntry::CheckEOFRecord,
                                base::Unretained(synchronous_entry_),
                                stream_index,
                                data_size_[stream_index],
                                crc32s_[stream_index],
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
    RecordReadResult(cache_type_, READ_RESULT_SYNC_READ_FAILURE);
  } else {
    RecordReadResult(cache_type_, READ_RESULT_SUCCESS);
    if (crc_check_state_[stream_index] == CRC_CHECK_NEVER_READ_TO_END &&
        offset + *result == GetDataSize(stream_index)) {
      crc_check_state_[stream_index] = CRC_CHECK_NOT_DONE;
    }
  }
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_END,
        CreateNetLogReadWriteCompleteCallback(*result));
  }

  EntryOperationComplete(
      stream_index,
      completion_callback,
      SimpleEntryStat(*last_used, last_modified_, data_size_),
      result.Pass());
}

void SimpleEntryImpl::WriteOperationComplete(
    int stream_index,
    const CompletionCallback& completion_callback,
    scoped_ptr<SimpleEntryStat> entry_stat,
    scoped_ptr<int> result) {
  if (*result >= 0)
    RecordWriteResult(cache_type_, WRITE_RESULT_SUCCESS);
  else
    RecordWriteResult(cache_type_, WRITE_RESULT_SYNC_WRITE_FAILURE);
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_WRITE_END,
        CreateNetLogReadWriteCompleteCallback(*result));
  }

  EntryOperationComplete(
      stream_index, completion_callback, *entry_stat, result.Pass());
}

void SimpleEntryImpl::DoomOperationComplete(const CompletionCallback& callback,
                                            State state_to_restore,
                                            int result) {
  state_ = state_to_restore;
  if (!callback.is_null())
    callback.Run(result);
  RunNextOperationIfNeeded();
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

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEventWithNetErrorCode(
        net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CHECKSUM_END,
        *result);
  }

  if (*result == net::OK) {
    *result = orig_result;
    if (orig_result >= 0)
      RecordReadResult(cache_type_, READ_RESULT_SUCCESS);
    else
      RecordReadResult(cache_type_, READ_RESULT_SYNC_READ_FAILURE);
  } else {
    RecordReadResult(cache_type_, READ_RESULT_SYNC_CHECKSUM_FAILURE);
  }
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_READ_END,
        CreateNetLogReadWriteCompleteCallback(*result));
  }

  EntryOperationComplete(
      stream_index,
      completion_callback,
      SimpleEntryStat(last_used_, last_modified_, data_size_),
      result.Pass());
}

void SimpleEntryImpl::CloseOperationComplete() {
  DCHECK(!synchronous_entry_);
  DCHECK_EQ(0, open_count_);
  DCHECK(STATE_IO_PENDING == state_ || STATE_FAILURE == state_ ||
         STATE_UNINITIALIZED == state_);
  net_log_.AddEvent(net::NetLog::TYPE_SIMPLE_CACHE_ENTRY_CLOSE_END);
  AdjustOpenEntryCountBy(cache_type_, -1);
  MakeUninitialized();
  RunNextOperationIfNeeded();
}

void SimpleEntryImpl::UpdateDataFromEntryStat(
    const SimpleEntryStat& entry_stat) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(synchronous_entry_);
  DCHECK_EQ(STATE_READY, state_);

  last_used_ = entry_stat.last_used;
  last_modified_ = entry_stat.last_modified;
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    data_size_[i] = entry_stat.data_size[i];
  }
  if (backend_.get())
    backend_->index()->UpdateEntrySize(entry_hash_, GetDiskUsage());
}

int64 SimpleEntryImpl::GetDiskUsage() const {
  int64 file_size = 0;
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    file_size +=
        simple_util::GetFileSizeFromKeyAndDataSize(key_, data_size_[i]);
  }
  return file_size;
}

void SimpleEntryImpl::RecordReadIsParallelizable(
    const SimpleEntryOperation& operation) const {
  if (!executing_operation_)
    return;
  // Used in histograms, please only add entries at the end.
  enum ReadDependencyType {
    // READ_STANDALONE = 0, Deprecated.
    READ_FOLLOWS_READ = 1,
    READ_FOLLOWS_CONFLICTING_WRITE = 2,
    READ_FOLLOWS_NON_CONFLICTING_WRITE = 3,
    READ_FOLLOWS_OTHER = 4,
    READ_ALONE_IN_QUEUE = 5,
    READ_DEPENDENCY_TYPE_MAX = 6,
  };

  ReadDependencyType type = READ_FOLLOWS_OTHER;
  if (operation.alone_in_queue()) {
    type = READ_ALONE_IN_QUEUE;
  } else if (executing_operation_->type() == SimpleEntryOperation::TYPE_READ) {
    type = READ_FOLLOWS_READ;
  } else if (executing_operation_->type() == SimpleEntryOperation::TYPE_WRITE) {
    if (executing_operation_->ConflictsWith(operation))
      type = READ_FOLLOWS_CONFLICTING_WRITE;
    else
      type = READ_FOLLOWS_NON_CONFLICTING_WRITE;
  }
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "ReadIsParallelizable", cache_type_,
                   type, READ_DEPENDENCY_TYPE_MAX);
}

void SimpleEntryImpl::RecordWriteDependencyType(
    const SimpleEntryOperation& operation) const {
  if (!executing_operation_)
    return;
  // Used in histograms, please only add entries at the end.
  enum WriteDependencyType {
    WRITE_OPTIMISTIC = 0,
    WRITE_FOLLOWS_CONFLICTING_OPTIMISTIC = 1,
    WRITE_FOLLOWS_NON_CONFLICTING_OPTIMISTIC = 2,
    WRITE_FOLLOWS_CONFLICTING_WRITE = 3,
    WRITE_FOLLOWS_NON_CONFLICTING_WRITE = 4,
    WRITE_FOLLOWS_CONFLICTING_READ = 5,
    WRITE_FOLLOWS_NON_CONFLICTING_READ = 6,
    WRITE_FOLLOWS_OTHER = 7,
    WRITE_DEPENDENCY_TYPE_MAX = 8,
  };

  WriteDependencyType type = WRITE_FOLLOWS_OTHER;
  if (operation.optimistic()) {
    type = WRITE_OPTIMISTIC;
  } else if (executing_operation_->type() == SimpleEntryOperation::TYPE_READ ||
             executing_operation_->type() == SimpleEntryOperation::TYPE_WRITE) {
    bool conflicting = executing_operation_->ConflictsWith(operation);

    if (executing_operation_->type() == SimpleEntryOperation::TYPE_READ) {
      type = conflicting ? WRITE_FOLLOWS_CONFLICTING_READ
                         : WRITE_FOLLOWS_NON_CONFLICTING_READ;
    } else if (executing_operation_->optimistic()) {
      type = conflicting ? WRITE_FOLLOWS_CONFLICTING_OPTIMISTIC
                         : WRITE_FOLLOWS_NON_CONFLICTING_OPTIMISTIC;
    } else {
      type = conflicting ? WRITE_FOLLOWS_CONFLICTING_WRITE
                         : WRITE_FOLLOWS_NON_CONFLICTING_WRITE;
    }
  }
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "WriteDependencyType", cache_type_,
                   type, WRITE_DEPENDENCY_TYPE_MAX);
}

}  // namespace disk_cache
