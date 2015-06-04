// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/shared_memory_data_consumer_handle.h"

#include <algorithm>
#include <deque>
#include <vector>

#include "content/public/child/fixed_received_data.h"

namespace content {

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
    for (RequestPeer::ReceivedData* data : queue_) {
      delete data;
    }
    queue_.clear();
    first_offset_ = 0;
    client_ = nullptr;
  }
  RequestPeer::ReceivedData* Top() { return queue_.front(); }
  void Push(scoped_ptr<RequestPeer::ReceivedData> data) {
    queue_.push_back(data.release());
  }
  size_t first_offset() const { return first_offset_; }
  Result result() const { return result_; }
  void set_result(Result r) { result_ = r; }
  Client* client() { return client_; }
  void set_client(Client* client) { client_ = client; }
  bool is_reader_active() const { return is_reader_active_; }
  void set_is_reader_active(bool b) { is_reader_active_ = b; }
  void Consume(size_t s) {
    first_offset_ += s;
    RequestPeer::ReceivedData* top = Top();
    if (static_cast<size_t>(top->length()) <= first_offset_) {
      delete top;
      queue_.pop_front();
      first_offset_ = 0;
    }
  }

 private:
  friend class base::RefCountedThreadSafe<Context>;
  ~Context() {
    // This is necessary because the queue stores raw pointers.
    Clear();
  }

  // |result_| stores the ultimate state of this handle if it has. Otherwise,
  // |Ok| is set.
  Result result_;
  // TODO(yhirano): Use std::deque<scoped_ptr<ReceivedData>> once it is allowed.
  std::deque<RequestPeer::ReceivedData*> queue_;
  size_t first_offset_;
  Client* client_;
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

  if (!context_->is_reader_active()) {
    // No one is interested in the data.
    return;
  }

  bool needs_notification = context_->client() && context_->IsEmpty();
  scoped_ptr<RequestPeer::ReceivedData> data_to_pass;
  if (mode_ == kApplyBackpressure) {
    data_to_pass = data.Pass();
  } else {
    data_to_pass = make_scoped_ptr(new FixedReceivedData(data.get()));
  }
  context_->Push(data_to_pass.Pass());

  if (needs_notification)
    context_->client()->didGetReadable();
}

void SharedMemoryDataConsumerHandle::Writer::Close() {
  if (context_->result() == Ok) {
    context_->set_result(Done);
    if (context_->client() && context_->IsEmpty())
      context_->client()->didGetReadable();
  }
}

SharedMemoryDataConsumerHandle::SharedMemoryDataConsumerHandle(
    BackpressureMode mode,
    scoped_ptr<Writer>* writer)
    : context_(new Context) {
  writer->reset(new Writer(context_, mode));
}

SharedMemoryDataConsumerHandle::~SharedMemoryDataConsumerHandle() {
  context_->set_is_reader_active(false);
  context_->Clear();
}

Result SharedMemoryDataConsumerHandle::read(void* data,
                                            size_t size,
                                            Flags flags,
                                            size_t* read_size_to_return) {
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
  if (context_->IsEmpty())
    return UnexpectedError;

  context_->Consume(read_size);
  return Ok;
}

void SharedMemoryDataConsumerHandle::registerClient(Client* client) {
  context_->set_client(client);

  if (!context_->IsEmpty())
    client->didGetReadable();
}

void SharedMemoryDataConsumerHandle::unregisterClient() {
  context_->set_client(nullptr);
}

}  // namespace content
