// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_sink_receiver.h"

#include <limits>

#include "base/bind.h"
#include "device/serial/async_waiter.h"

namespace device {

// Represents a flush of data that has not been completed.
class DataSinkReceiver::PendingFlush {
 public:
  PendingFlush();

  // Initializes this PendingFlush with |num_bytes|, the number of bytes to
  // flush.
  void SetNumBytesToFlush(uint32_t num_bytes);

  // Attempts to discard |bytes_to_flush_| bytes from |handle|. Returns
  // MOJO_RESULT_OK on success, MOJO_RESULT_SHOULD_WAIT if fewer than
  // |bytes_to_flush_| bytes were flushed or the error if one is encountered
  // discarding data from |handle|.
  MojoResult Flush(mojo::DataPipeConsumerHandle handle);

  // Whether this PendingFlush has received the number of bytes to flush.
  bool received_flush() { return received_flush_; }

 private:
  // Whether this PendingFlush has received the number of bytes to flush.
  bool received_flush_;

  // The remaining number of bytes to flush.
  uint32_t bytes_to_flush_;
};

// A ReadOnlyBuffer implementation that provides a view of a data pipe owned by
// a DataSinkReceiver.
class DataSinkReceiver::Buffer : public ReadOnlyBuffer {
 public:
  Buffer(scoped_refptr<DataSinkReceiver> receiver,
         const char* buffer,
         uint32_t buffer_size);
  virtual ~Buffer();

  void Cancel(int32_t error);

  // ReadOnlyBuffer overrides.
  virtual const char* GetData() OVERRIDE;
  virtual uint32_t GetSize() OVERRIDE;
  virtual void Done(uint32_t bytes_read) OVERRIDE;
  virtual void DoneWithError(uint32_t bytes_read, int32_t error) OVERRIDE;

 private:
  // The DataSinkReceiver whose data pipe we are providing a view.
  scoped_refptr<DataSinkReceiver> receiver_;

  const char* buffer_;
  uint32_t buffer_size_;

  // Whether this receive has been cancelled.
  bool cancelled_;

  // If |cancelled_|, contains the cancellation error to report.
  int32_t cancellation_error_;
};

DataSinkReceiver::DataSinkReceiver(const ReadyCallback& ready_callback,
                                   const CancelCallback& cancel_callback,
                                   const ErrorCallback& error_callback)
    : ready_callback_(ready_callback),
      cancel_callback_(cancel_callback),
      error_callback_(error_callback),
      buffer_in_use_(NULL),
      shut_down_(false),
      weak_factory_(this) {
}

void DataSinkReceiver::ShutDown() {
  shut_down_ = true;
  if (waiter_)
    waiter_.reset();
}

DataSinkReceiver::~DataSinkReceiver() {
}

void DataSinkReceiver::Init(mojo::ScopedDataPipeConsumerHandle handle) {
  if (!handle.is_valid() || handle_.is_valid()) {
    DispatchFatalError();
    return;
  }

  handle_ = handle.Pass();
  StartWaiting();
}

void DataSinkReceiver::Cancel(int32_t error) {
  // If we have sent a ReportBytesSentAndError but have not received the
  // response, that ReportBytesSentAndError message will appear to the
  // DataSinkClient to be caused by this Cancel message. In that case, we ignore
  // the cancel.
  if (!pending_flushes_.empty() && !pending_flushes_.back()->received_flush())
    return;

  // If there is a buffer is in use, mark the buffer as cancelled and notify the
  // client by calling |cancel_callback_|. The sink implementation may or may
  // not take the cancellation into account when deciding what error (if any) to
  // return. If the sink returns an error, we ignore the cancellation error.
  // Otherwise, if the sink does not report an error, we override that with the
  // cancellation error. Once a cancellation has been received, the next report
  // sent to the client will always contain an error; the error returned by the
  // sink or the cancellation error if the sink does not return an error.
  if (buffer_in_use_) {
    buffer_in_use_->Cancel(error);
    if (!cancel_callback_.is_null())
      cancel_callback_.Run(error);
    return;
  }
  // If there is no buffer in use, immediately report the error and cancel the
  // waiting for the data pipe if one exists. This transitions straight into the
  // state after the sink has returned an error.
  waiter_.reset();
  ReportBytesSentAndError(0, error);
}

void DataSinkReceiver::OnConnectionError() {
  DispatchFatalError();
}

void DataSinkReceiver::StartWaiting() {
  DCHECK(!waiter_ && !shut_down_);
  waiter_.reset(
      new AsyncWaiter(handle_.get(),
                      MOJO_HANDLE_SIGNAL_READABLE,
                      base::Bind(&DataSinkReceiver::OnDoneWaiting, this)));
}

void DataSinkReceiver::OnDoneWaiting(MojoResult result) {
  DCHECK(waiter_ && !shut_down_);
  waiter_.reset();
  if (result != MOJO_RESULT_OK) {
    DispatchFatalError();
    return;
  }
  // If there are any queued flushes (from ReportBytesSentAndError()), let them
  // flush data from the data pipe.
  if (!pending_flushes_.empty()) {
    MojoResult result = pending_flushes_.front()->Flush(handle_.get());
    if (result == MOJO_RESULT_OK) {
      pending_flushes_.pop();
    } else if (result != MOJO_RESULT_SHOULD_WAIT) {
      DispatchFatalError();
      return;
    }
    StartWaiting();
    return;
  }
  const void* data = NULL;
  uint32_t num_bytes = std::numeric_limits<uint32_t>::max();
  result = mojo::BeginReadDataRaw(
      handle_.get(), &data, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    DispatchFatalError();
    return;
  }
  buffer_in_use_ = new Buffer(this, static_cast<const char*>(data), num_bytes);
  ready_callback_.Run(scoped_ptr<ReadOnlyBuffer>(buffer_in_use_));
}

void DataSinkReceiver::Done(uint32_t bytes_read) {
  if (!DoneInternal(bytes_read))
    return;
  client()->ReportBytesSent(bytes_read);
  StartWaiting();
}

void DataSinkReceiver::DoneWithError(uint32_t bytes_read, int32_t error) {
  if (!DoneInternal(bytes_read))
    return;
  ReportBytesSentAndError(bytes_read, error);
}

bool DataSinkReceiver::DoneInternal(uint32_t bytes_read) {
  if (shut_down_)
    return false;

  DCHECK(buffer_in_use_);
  buffer_in_use_ = NULL;
  MojoResult result = mojo::EndReadDataRaw(handle_.get(), bytes_read);
  if (result != MOJO_RESULT_OK) {
    DispatchFatalError();
    return false;
  }
  return true;
}

void DataSinkReceiver::ReportBytesSentAndError(uint32_t bytes_read,
                                               int32_t error) {
  // When we encounter an error, we must discard the data from any sends already
  // in the data pipe before we can resume dispatching data to the sink. We add
  // a pending flush here. The response containing the number of bytes to flush
  // is handled in SetNumBytesToFlush(). The actual flush is handled in
  // OnDoneWaiting().
  pending_flushes_.push(linked_ptr<PendingFlush>(new PendingFlush()));
  client()->ReportBytesSentAndError(
      bytes_read,
      error,
      base::Bind(&DataSinkReceiver::SetNumBytesToFlush,
                 weak_factory_.GetWeakPtr()));
}

void DataSinkReceiver::SetNumBytesToFlush(uint32_t bytes_to_flush) {
  DCHECK(!pending_flushes_.empty());
  DCHECK(!pending_flushes_.back()->received_flush());
  pending_flushes_.back()->SetNumBytesToFlush(bytes_to_flush);
  if (!waiter_)
    StartWaiting();
}

void DataSinkReceiver::DispatchFatalError() {
  if (shut_down_)
    return;

  ShutDown();
  if (!error_callback_.is_null())
    error_callback_.Run();
}

DataSinkReceiver::Buffer::Buffer(scoped_refptr<DataSinkReceiver> receiver,
                                 const char* buffer,
                                 uint32_t buffer_size)
    : receiver_(receiver),
      buffer_(buffer),
      buffer_size_(buffer_size),
      cancelled_(false),
      cancellation_error_(0) {
}

DataSinkReceiver::Buffer::~Buffer() {
  if (!receiver_)
    return;
  if (cancelled_)
    receiver_->DoneWithError(0, cancellation_error_);
  else
    receiver_->Done(0);
}

void DataSinkReceiver::Buffer::Cancel(int32_t error) {
  cancelled_ = true;
  cancellation_error_ = error;
}

const char* DataSinkReceiver::Buffer::GetData() {
  return buffer_;
}

uint32_t DataSinkReceiver::Buffer::GetSize() {
  return buffer_size_;
}

void DataSinkReceiver::Buffer::Done(uint32_t bytes_read) {
  if (cancelled_)
    receiver_->DoneWithError(bytes_read, cancellation_error_);
  else
    receiver_->Done(bytes_read);
  receiver_ = NULL;
  buffer_ = NULL;
  buffer_size_ = 0;
}

void DataSinkReceiver::Buffer::DoneWithError(uint32_t bytes_read,
                                             int32_t error) {
  receiver_->DoneWithError(bytes_read, error);
  receiver_ = NULL;
  buffer_ = NULL;
  buffer_size_ = 0;
}

DataSinkReceiver::PendingFlush::PendingFlush()
    : received_flush_(false), bytes_to_flush_(0) {
}

void DataSinkReceiver::PendingFlush::SetNumBytesToFlush(uint32_t num_bytes) {
  DCHECK(!received_flush_);
  received_flush_ = true;
  bytes_to_flush_ = num_bytes;
}

MojoResult DataSinkReceiver::PendingFlush::Flush(
    mojo::DataPipeConsumerHandle handle) {
  DCHECK(received_flush_);
  uint32_t num_bytes = bytes_to_flush_;
  MojoResult result =
      mojo::ReadDataRaw(handle, NULL, &num_bytes, MOJO_READ_DATA_FLAG_DISCARD);
  if (result != MOJO_RESULT_OK)
    return result;
  DCHECK(num_bytes <= bytes_to_flush_);
  bytes_to_flush_ -= num_bytes;
  return bytes_to_flush_ == 0 ? MOJO_RESULT_OK : MOJO_RESULT_SHOULD_WAIT;
}

}  // namespace device
