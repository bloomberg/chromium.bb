// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_sink_receiver.h"

#include <limits>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace device {

// A ReadOnlyBuffer implementation that provides a view of a buffer owned by a
// DataSinkReceiver.
class DataSinkReceiver::Buffer : public ReadOnlyBuffer {
 public:
  Buffer(scoped_refptr<DataSinkReceiver> receiver,
         const char* buffer,
         uint32_t buffer_size);
  ~Buffer() override;

  void Cancel(int32_t error);

  // ReadOnlyBuffer overrides.
  const char* GetData() override;
  uint32_t GetSize() override;
  void Done(uint32_t bytes_read) override;
  void DoneWithError(uint32_t bytes_read, int32_t error) override;

 private:
  // The DataSinkReceiver of whose buffer we are providing a view.
  scoped_refptr<DataSinkReceiver> receiver_;

  const char* buffer_;
  uint32_t buffer_size_;

  // Whether this receive has been cancelled.
  bool cancelled_;

  // If |cancelled_|, contains the cancellation error to report.
  int32_t cancellation_error_;
};

// A frame of data received from the client.
class DataSinkReceiver::DataFrame {
 public:
  explicit DataFrame(mojo::Array<uint8_t> data);

  // Returns the number of uncomsumed bytes remaining of this data frame.
  uint32_t GetRemainingBytes();

  // Returns a pointer to the remaining data to be consumed.
  const char* GetData();

  // Reports that |bytes_read| bytes have been consumed.
  void OnDataConsumed(uint32_t bytes_read);

 private:
  mojo::Array<uint8_t> data_;
  uint32_t offset_;
};

DataSinkReceiver::DataSinkReceiver(const ReadyCallback& ready_callback,
                                   const CancelCallback& cancel_callback,
                                   const ErrorCallback& error_callback)
    : ready_callback_(ready_callback),
      cancel_callback_(cancel_callback),
      error_callback_(error_callback),
      flush_pending_(false),
      buffer_in_use_(NULL),
      initialized_(false),
      available_buffer_capacity_(0),
      shut_down_(false),
      weak_factory_(this) {
}

void DataSinkReceiver::ShutDown() {
  shut_down_ = true;
}

DataSinkReceiver::~DataSinkReceiver() {
}

void DataSinkReceiver::Init(uint32_t buffer_size) {
  if (initialized_) {
    ShutDown();
    return;
  }
  initialized_ = true;
  available_buffer_capacity_ = buffer_size;
}

void DataSinkReceiver::Cancel(int32_t error) {
  // If we have sent a ReportBytesSentAndError but have not received the
  // response, that ReportBytesSentAndError message will appear to the
  // DataSinkClient to be caused by this Cancel message. In that case, we ignore
  // the cancel.
  if (flush_pending_)
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
  ReportBytesSentAndError(0, error);
}

void DataSinkReceiver::OnData(mojo::Array<uint8_t> data) {
  if (!initialized_) {
    ShutDown();
    return;
  }
  if (data.size() > available_buffer_capacity_) {
    ShutDown();
    return;
  }
  available_buffer_capacity_ -= static_cast<uint32_t>(data.size());
  pending_data_buffers_.push(linked_ptr<DataFrame>(new DataFrame(data.Pass())));
  if (!buffer_in_use_ && !flush_pending_)
    RunReadyCallback();
}

void DataSinkReceiver::OnConnectionError() {
  DispatchFatalError();
}

void DataSinkReceiver::RunReadyCallback() {
  DCHECK(!shut_down_ && !flush_pending_);
  // If data arrives while a call to RunReadyCallback() is posted, we can be
  // called with buffer_in_use_ already set.
  if (buffer_in_use_)
    return;
  buffer_in_use_ =
      new Buffer(this,
                 pending_data_buffers_.front()->GetData(),
                 pending_data_buffers_.front()->GetRemainingBytes());
  ready_callback_.Run(scoped_ptr<ReadOnlyBuffer>(buffer_in_use_));
}

void DataSinkReceiver::Done(uint32_t bytes_read) {
  if (!DoneInternal(bytes_read))
    return;
  client()->ReportBytesSent(bytes_read);
  if (!pending_data_buffers_.empty()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&DataSinkReceiver::RunReadyCallback,
                   weak_factory_.GetWeakPtr()));
  }
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
  available_buffer_capacity_ += bytes_read;
  pending_data_buffers_.front()->OnDataConsumed(bytes_read);
  if (pending_data_buffers_.front()->GetRemainingBytes() == 0)
    pending_data_buffers_.pop();
  return true;
}

void DataSinkReceiver::ReportBytesSentAndError(uint32_t bytes_read,
                                               int32_t error) {
  // When we encounter an error, we must discard the data from any send buffers
  // transmitted by the DataSinkClient before it receives this error.
  flush_pending_ = true;
  client()->ReportBytesSentAndError(
      bytes_read,
      error,
      base::Bind(&DataSinkReceiver::DoFlush, weak_factory_.GetWeakPtr()));
}

void DataSinkReceiver::DoFlush() {
  DCHECK(flush_pending_);
  flush_pending_ = false;
  while (!pending_data_buffers_.empty()) {
    available_buffer_capacity_ +=
        pending_data_buffers_.front()->GetRemainingBytes();
    pending_data_buffers_.pop();
  }
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
  if (!receiver_.get())
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
  scoped_refptr<DataSinkReceiver> receiver = receiver_;
  receiver_ = nullptr;
  if (cancelled_)
    receiver->DoneWithError(bytes_read, cancellation_error_);
  else
    receiver->Done(bytes_read);
  buffer_ = NULL;
  buffer_size_ = 0;
}

void DataSinkReceiver::Buffer::DoneWithError(uint32_t bytes_read,
                                             int32_t error) {
  scoped_refptr<DataSinkReceiver> receiver = receiver_;
  receiver_ = nullptr;
  receiver->DoneWithError(bytes_read, error);
  buffer_ = NULL;
  buffer_size_ = 0;
}

DataSinkReceiver::DataFrame::DataFrame(mojo::Array<uint8_t> data)
    : data_(data.Pass()), offset_(0) {
  DCHECK_LT(0u, data_.size());
}

// Returns the number of uncomsumed bytes remaining of this data frame.
uint32_t DataSinkReceiver::DataFrame::GetRemainingBytes() {
  return static_cast<uint32_t>(data_.size() - offset_);
}

// Returns a pointer to the remaining data to be consumed.
const char* DataSinkReceiver::DataFrame::GetData() {
  DCHECK_LT(offset_, data_.size());
  return reinterpret_cast<const char*>(&data_[0]) + offset_;
}

void DataSinkReceiver::DataFrame::OnDataConsumed(uint32_t bytes_read) {
  offset_ += bytes_read;
  DCHECK_LE(offset_, data_.size());
}

}  // namespace device
