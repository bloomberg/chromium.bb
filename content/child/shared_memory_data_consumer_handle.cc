// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_memory_data_consumer_handle.h"

#include <algorithm>
#include <deque>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "content/public/child/fixed_received_data.h"

namespace content {

namespace {

class DelegateThreadSafeReceivedData final
    : public RequestPeer::ThreadSafeReceivedData {
 public:
  explicit DelegateThreadSafeReceivedData(
      scoped_ptr<RequestPeer::ReceivedData> data)
      : data_(data.Pass()),
        task_runner_(base::MessageLoop::current()->task_runner()) {}
  ~DelegateThreadSafeReceivedData() override {
    if (!task_runner_->BelongsToCurrentThread()) {
      // Delete the data on the original thread.
      task_runner_->DeleteSoon(FROM_HERE, data_.release());
    }
  }

  const char* payload() const override { return data_->payload(); }
  int length() const override { return data_->length(); }
  int encoded_length() const override { return data_->encoded_length(); }

 private:
  scoped_ptr<RequestPeer::ReceivedData> data_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DelegateThreadSafeReceivedData);
};

}  // namespace

using Result = blink::WebDataConsumerHandle::Result;

class SharedMemoryDataConsumerHandle::Context final
    : public base::RefCountedThreadSafe<Context> {
 public:
  Context()
      : result_(Ok),
        first_offset_(0),
        client_(nullptr),
        is_reader_active_(true) {}

  bool IsEmpty() const { return queue_.empty(); }
  void Clear() {
    for (auto& data : queue_) {
      delete data;
    }
    queue_.clear();
    first_offset_ = 0;
    client_ = nullptr;
  }
  void Notify() {
    // Note that this function is not protected by |lock_| (actually it
    // shouldn't be) but |notification_task_runner_| is thread-safe.

    if (notification_task_runner_->BelongsToCurrentThread()) {
      NotifyImmediately();
    } else {
      notification_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Context::NotifyImmediately, this));
    }
  }

  RequestPeer::ThreadSafeReceivedData* Top() { return queue_.front(); }
  void Push(scoped_ptr<RequestPeer::ThreadSafeReceivedData> data) {
    queue_.push_back(data.release());
  }
  size_t first_offset() const { return first_offset_; }
  Result result() const { return result_; }
  void set_result(Result r) { result_ = r; }
  Client* client() { return client_; }
  void SetClient(Client* client) {
    if (client) {
      notification_task_runner_ = base::MessageLoop::current()->task_runner();
      client_ = client;
    } else {
      notification_task_runner_ = nullptr;
      client_ = nullptr;
    }
  }
  bool is_reader_active() const { return is_reader_active_; }
  void set_is_reader_active(bool b) { is_reader_active_ = b; }
  void Consume(size_t s) {
    first_offset_ += s;
    auto top = Top();
    if (static_cast<size_t>(top->length()) <= first_offset_) {
      delete top;
      queue_.pop_front();
      first_offset_ = 0;
    }
  }
  base::Lock& lock() { return lock_; }

 private:
  friend class base::RefCountedThreadSafe<Context>;
  ~Context() {
    // This is necessary because the queue stores raw pointers.
    Clear();
  }

  void NotifyImmediately() {
    // As we can assume that all reader-side methods are called on this
    // thread (see WebDataConsumerHandle comments), we don't need to lock.
    if (client_)
      client_->didGetReadable();
  }

  base::Lock lock_;
  // |result_| stores the ultimate state of this handle if it has. Otherwise,
  // |Ok| is set.
  Result result_;
  // TODO(yhirano): Use std::deque<scoped_ptr<ThreadSafeReceivedData>>
  // once it is allowed.
  std::deque<RequestPeer::ThreadSafeReceivedData*> queue_;
  size_t first_offset_;
  Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> notification_task_runner_;
  bool is_reader_active_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

SharedMemoryDataConsumerHandle::Writer::Writer(
    const scoped_refptr<Context>& context,
    BackpressureMode mode)
    : context_(context), mode_(mode) {
}

SharedMemoryDataConsumerHandle::Writer::~Writer() {
  Close();
}

void SharedMemoryDataConsumerHandle::Writer::AddData(
    scoped_ptr<RequestPeer::ReceivedData> data) {
  if (!data->length()) {
    // We omit empty data.
    return;
  }

  bool needs_notification = false;
  {
    base::AutoLock lock(context_->lock());
    if (!context_->is_reader_active()) {
      // No one is interested in the data.
      return;
    }

    needs_notification = context_->client() && context_->IsEmpty();
    scoped_ptr<RequestPeer::ThreadSafeReceivedData> data_to_pass;
    if (mode_ == kApplyBackpressure) {
      data_to_pass =
          make_scoped_ptr(new DelegateThreadSafeReceivedData(data.Pass()));
    } else {
      data_to_pass = make_scoped_ptr(new FixedReceivedData(data.get()));
    }
    context_->Push(data_to_pass.Pass());
  }

  if (needs_notification)
    context_->Notify();
}

void SharedMemoryDataConsumerHandle::Writer::Close() {
  bool needs_notification = false;

  {
    base::AutoLock lock(context_->lock());
    if (context_->result() == Ok) {
      context_->set_result(Done);
      needs_notification = context_->client() && context_->IsEmpty();
    }
  }
  if (needs_notification)
    context_->Notify();
}

SharedMemoryDataConsumerHandle::SharedMemoryDataConsumerHandle(
    BackpressureMode mode,
    scoped_ptr<Writer>* writer)
    : context_(new Context) {
  writer->reset(new Writer(context_, mode));
}

SharedMemoryDataConsumerHandle::~SharedMemoryDataConsumerHandle() {
  base::AutoLock lock(context_->lock());
  context_->set_is_reader_active(false);
  context_->Clear();
}

Result SharedMemoryDataConsumerHandle::read(void* data,
                                            size_t size,
                                            Flags flags,
                                            size_t* read_size_to_return) {
  base::AutoLock lock(context_->lock());

  size_t total_read_size = 0;
  *read_size_to_return = 0;
  if (context_->result() != Ok && context_->result() != Done)
    return context_->result();

  while (!context_->IsEmpty() && total_read_size < size) {
    const auto& top = context_->Top();
    size_t readable = top->length() - context_->first_offset();
    size_t writable = size - total_read_size;
    size_t read_size = std::min(readable, writable);
    const char* begin = top->payload() + context_->first_offset();
    std::copy(begin, begin + read_size,
              static_cast<char*>(data) + total_read_size);
    total_read_size += read_size;
    context_->Consume(read_size);
  }
  *read_size_to_return = total_read_size;
  return total_read_size ? Ok : context_->result() == Done ? Done : ShouldWait;
}

Result SharedMemoryDataConsumerHandle::beginRead(const void** buffer,
                                                 Flags flags,
                                                 size_t* available) {
  *buffer = nullptr;
  *available = 0;

  base::AutoLock lock(context_->lock());

  if (context_->result() != Ok && context_->result() != Done)
    return context_->result();

  if (context_->IsEmpty())
    return context_->result() == Done ? Done : ShouldWait;

  const auto& top = context_->Top();
  *buffer = top->payload() + context_->first_offset();
  *available = top->length() - context_->first_offset();

  return Ok;
}

Result SharedMemoryDataConsumerHandle::endRead(size_t read_size) {
  base::AutoLock lock(context_->lock());

  if (context_->IsEmpty())
    return UnexpectedError;

  context_->Consume(read_size);
  return Ok;
}

void SharedMemoryDataConsumerHandle::registerClient(Client* client) {
  bool needs_notification = false;
  {
    base::AutoLock lock(context_->lock());

    context_->SetClient(client);
    needs_notification = !context_->IsEmpty();
  }
  if (needs_notification)
    context_->Notify();
}

void SharedMemoryDataConsumerHandle::unregisterClient() {
  base::AutoLock lock(context_->lock());

  context_->SetClient(nullptr);
}

}  // namespace content
