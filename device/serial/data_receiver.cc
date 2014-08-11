// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_receiver.h"

#include <limits>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "device/serial/async_waiter.h"

namespace device {

// Represents a receive that is not yet fulfilled.
class DataReceiver::PendingReceive {
 public:
  PendingReceive(DataReceiver* receiver,
                 const ReceiveDataCallback& callback,
                 const ReceiveErrorCallback& error_callback,
                 int32_t fatal_error_value);

  // Dispatches |data| to |receive_callback_|.
  void DispatchData(const void* data, uint32_t num_bytes);

  // Reports |error| to |receive_error_callback_| if it is an appropriate time.
  // Returns whether it dispatched |error|.
  bool DispatchError(DataReceiver::PendingError* error,
                     uint32_t bytes_received);

  // Reports |fatal_error_value_| to |receive_error_callback_|.
  void DispatchFatalError();

 private:
  class Buffer;

  // Invoked when the user is finished with the ReadOnlyBuffer provided to
  // |receive_callback_|.
  void Done(uint32_t num_bytes);

  // The DataReceiver that owns this.
  DataReceiver* receiver_;

  // The callback to dispatch data.
  ReceiveDataCallback receive_callback_;

  // The callback to report errors.
  ReceiveErrorCallback receive_error_callback_;

  // The error value to report when DispatchFatalError() is called.
  const int32_t fatal_error_value_;

  // True if the user owns a buffer passed to |receive_callback_| as part of
  // DispatchData().
  bool buffer_in_use_;
};

// A ReadOnlyBuffer implementation that provides a view of a data pipe owned by
// a DataReceiver.
class DataReceiver::PendingReceive::Buffer : public ReadOnlyBuffer {
 public:
  Buffer(scoped_refptr<DataReceiver> pipe,
         PendingReceive* receive,
         const char* buffer,
         uint32_t buffer_size);
  virtual ~Buffer();

  // ReadOnlyBuffer overrides.
  virtual const char* GetData() OVERRIDE;
  virtual uint32_t GetSize() OVERRIDE;
  virtual void Done(uint32_t bytes_consumed) OVERRIDE;
  virtual void DoneWithError(uint32_t bytes_consumed, int32_t error) OVERRIDE;

 private:
  // The DataReceiver whose data pipe we are providing a view.
  scoped_refptr<DataReceiver> receiver_;

  // The PendingReceive to which this buffer has been created in response.
  PendingReceive* pending_receive_;

  const char* buffer_;
  uint32_t buffer_size_;
};

// Represents an error received from the DataSource.
struct DataReceiver::PendingError {
  PendingError(uint32_t offset, int32_t error)
      : offset(offset), error(error), dispatched(false) {}

  // The location within the data stream where the error occurred.
  const uint32_t offset;

  // The value of the error that occurred.
  const int32_t error;

  // Whether the error has been dispatched to the user.
  bool dispatched;
};

DataReceiver::DataReceiver(mojo::InterfacePtr<serial::DataSource> source,
                           uint32_t buffer_size,
                           int32_t fatal_error_value)
    : source_(source.Pass()),
      fatal_error_value_(fatal_error_value),
      bytes_received_(0),
      shut_down_(false),
      weak_factory_(this) {
  MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, buffer_size,
  };
  mojo::ScopedDataPipeProducerHandle remote_handle;
  MojoResult result = mojo::CreateDataPipe(&options, &remote_handle, &handle_);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  source_->Init(remote_handle.Pass());
  source_.set_client(this);
}

bool DataReceiver::Receive(const ReceiveDataCallback& callback,
                           const ReceiveErrorCallback& error_callback) {
  DCHECK(!callback.is_null() && !error_callback.is_null());
  if (pending_receive_ || shut_down_)
    return false;
  // When the DataSource encounters an error, it pauses transmission. When the
  // user starts a new receive following notification of the error (via
  // |error_callback| of the previous Receive call) of the error we can tell the
  // DataSource to resume transmission of data.
  if (pending_error_ && pending_error_->dispatched) {
    source_->Resume();
    pending_error_.reset();
  }

  pending_receive_.reset(
      new PendingReceive(this, callback, error_callback, fatal_error_value_));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DataReceiver::ReceiveInternal, weak_factory_.GetWeakPtr()));
  return true;
}

DataReceiver::~DataReceiver() {
  ShutDown();
}

void DataReceiver::OnError(uint32_t offset, int32_t error) {
  if (shut_down_)
    return;

  if (pending_error_) {
    // When OnError is called by the DataSource, transmission of data is
    // suspended. Thus we shouldn't receive another call to OnError until we
    // have fully dealt with the error and called Resume to resume transmission
    // (see Receive()). Under normal operation we should never get here, but if
    // we do (e.g. in the case of a hijacked service process) just shut down.
    ShutDown();
    return;
  }
  pending_error_.reset(new PendingError(offset, error));
  if (pending_receive_ &&
      pending_receive_->DispatchError(pending_error_.get(), bytes_received_)) {
    pending_receive_.reset();
    waiter_.reset();
  }
}

void DataReceiver::OnConnectionError() {
  ShutDown();
}

void DataReceiver::Done(uint32_t bytes_consumed) {
  if (shut_down_)
    return;

  DCHECK(pending_receive_);
  MojoResult result = mojo::EndReadDataRaw(handle_.get(), bytes_consumed);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  pending_receive_.reset();
  bytes_received_ += bytes_consumed;
}

void DataReceiver::OnDoneWaiting(MojoResult result) {
  DCHECK(pending_receive_ && !shut_down_ && waiter_);
  waiter_.reset();
  if (result != MOJO_RESULT_OK) {
    ShutDown();
    return;
  }
  ReceiveInternal();
}

void DataReceiver::ReceiveInternal() {
  if (shut_down_)
    return;
  DCHECK(pending_receive_);
  if (pending_error_ &&
      pending_receive_->DispatchError(pending_error_.get(), bytes_received_)) {
    pending_receive_.reset();
    waiter_.reset();
    return;
  }

  const void* data;
  uint32_t num_bytes = std::numeric_limits<uint32_t>::max();
  MojoResult result = mojo::BeginReadDataRaw(
      handle_.get(), &data, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    if (!CheckErrorNotInReadRange(num_bytes)) {
      ShutDown();
      return;
    }

    pending_receive_->DispatchData(data, num_bytes);
    return;
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    waiter_.reset(new AsyncWaiter(
        handle_.get(),
        MOJO_HANDLE_SIGNAL_READABLE,
        base::Bind(&DataReceiver::OnDoneWaiting, weak_factory_.GetWeakPtr())));
    return;
  }
  ShutDown();
}

bool DataReceiver::CheckErrorNotInReadRange(uint32_t num_bytes) {
  DCHECK(pending_receive_);
  if (!pending_error_)
    return true;

  DCHECK_NE(bytes_received_, pending_error_->offset);
  DCHECK_NE(num_bytes, 0u);
  uint32_t potential_bytes_received = bytes_received_ + num_bytes;
  // bytes_received_ can overflow so we must consider two cases:
  //   1. Both |bytes_received_| and |pending_error_->offset| have overflowed an
  //      equal number of times. In this case, |potential_bytes_received| must
  //      be in the range (|bytes_received|, |pending_error_->offset|]. Below
  //      this range can only occur if |bytes_received_| overflows before
  //      |pending_error_->offset|. Above can only occur if |bytes_received_|
  //      overtakes |pending_error_->offset|.
  //   2. |pending_error_->offset| has overflowed once more than
  //      |bytes_received_|. In this case, |potential_bytes_received| must not
  //      be in the range (|pending_error_->offset|, |bytes_received_|].
  if ((bytes_received_ < pending_error_->offset &&
       (potential_bytes_received > pending_error_->offset ||
        potential_bytes_received <= bytes_received_)) ||
      (bytes_received_ > pending_error_->offset &&
       potential_bytes_received > pending_error_->offset &&
       potential_bytes_received <= bytes_received_)) {
    return false;
  }
  return true;
}

void DataReceiver::ShutDown() {
  shut_down_ = true;
  if (pending_receive_)
    pending_receive_->DispatchFatalError();
  pending_error_.reset();
  waiter_.reset();
}

DataReceiver::PendingReceive::PendingReceive(
    DataReceiver* receiver,
    const ReceiveDataCallback& callback,
    const ReceiveErrorCallback& error_callback,
    int32_t fatal_error_value)
    : receiver_(receiver),
      receive_callback_(callback),
      receive_error_callback_(error_callback),
      fatal_error_value_(fatal_error_value),
      buffer_in_use_(false) {
}

void DataReceiver::PendingReceive::DispatchData(const void* data,
                                                uint32_t num_bytes) {
  DCHECK(!buffer_in_use_);
  buffer_in_use_ = true;
  receive_callback_.Run(scoped_ptr<ReadOnlyBuffer>(
      new Buffer(receiver_, this, static_cast<const char*>(data), num_bytes)));
}

bool DataReceiver::PendingReceive::DispatchError(PendingError* error,
                                                 uint32_t bytes_received) {
  DCHECK(!error->dispatched);
  if (buffer_in_use_ || bytes_received != error->offset)
    return false;

  error->dispatched = true;
  receive_error_callback_.Run(error->error);
  return true;
}

void DataReceiver::PendingReceive::DispatchFatalError() {
  receive_error_callback_.Run(fatal_error_value_);
}

void DataReceiver::PendingReceive::Done(uint32_t bytes_consumed) {
  DCHECK(buffer_in_use_);
  buffer_in_use_ = false;
  receiver_->Done(bytes_consumed);
}

DataReceiver::PendingReceive::Buffer::Buffer(
    scoped_refptr<DataReceiver> receiver,
    PendingReceive* receive,
    const char* buffer,
    uint32_t buffer_size)
    : receiver_(receiver),
      pending_receive_(receive),
      buffer_(buffer),
      buffer_size_(buffer_size) {
}

DataReceiver::PendingReceive::Buffer::~Buffer() {
  if (pending_receive_)
    pending_receive_->Done(0);
}

const char* DataReceiver::PendingReceive::Buffer::GetData() {
  return buffer_;
}

uint32_t DataReceiver::PendingReceive::Buffer::GetSize() {
  return buffer_size_;
}

void DataReceiver::PendingReceive::Buffer::Done(uint32_t bytes_consumed) {
  pending_receive_->Done(bytes_consumed);
  pending_receive_ = NULL;
  receiver_ = NULL;
  buffer_ = NULL;
  buffer_size_ = 0;
}

void DataReceiver::PendingReceive::Buffer::DoneWithError(
    uint32_t bytes_consumed,
    int32_t error) {
  Done(bytes_consumed);
}

}  // namespace device
