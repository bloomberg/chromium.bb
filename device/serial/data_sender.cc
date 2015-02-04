// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_sender.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace device {

// Represents a send that is not yet fulfilled.
class DataSender::PendingSend {
 public:
  PendingSend(const base::StringPiece& data,
              const DataSentCallback& callback,
              const SendErrorCallback& error_callback,
              int32_t fatal_error_value);

  // Invoked to report that |num_bytes| of data have been sent. Subtracts the
  // number of bytes that were part of this send from |num_bytes|. Returns
  // whether this send has been completed. If this send has been completed, this
  // calls |callback_|.
  bool ReportBytesSent(uint32_t* num_bytes);

  // Invoked to report that |num_bytes| of data have been sent and then an
  // error, |error| was encountered. Subtracts the number of bytes that were
  // part of this send from |num_bytes|. If this send was not completed before
  // the error, this calls |error_callback_| to report the error. Otherwise,
  // this calls |callback_|. Returns the number of bytes sent but not acked.
  uint32_t ReportBytesSentAndError(uint32_t* num_bytes, int32_t error);

  // Reports |fatal_error_value_| to |receive_error_callback_|.
  void DispatchFatalError();

  // Attempts to send any data not yet sent to |sink|.
  bool SendData(serial::DataSink* sink, uint32_t* available_buffer_size);

 private:
  // Invoked to update |bytes_acked_| and |num_bytes|.
  void ReportBytesSentInternal(uint32_t* num_bytes);

  // The data to send.
  const base::StringPiece data_;

  // The callback to report success.
  const DataSentCallback callback_;

  // The callback to report errors.
  const SendErrorCallback error_callback_;

  // The error value to report when DispatchFatalError() is called.
  const int32_t fatal_error_value_;

  // The number of bytes sent to the DataSink.
  uint32_t bytes_sent_;

  // The number of bytes acked.
  uint32_t bytes_acked_;
};

DataSender::DataSender(mojo::InterfacePtr<serial::DataSink> sink,
                       uint32_t buffer_size,
                       int32_t fatal_error_value)
    : sink_(sink.Pass()),
      fatal_error_value_(fatal_error_value),
      available_buffer_capacity_(buffer_size),
      shut_down_(false) {
  sink_.set_error_handler(this);
  sink_.set_client(this);
  sink_->Init(buffer_size);
}

DataSender::~DataSender() {
  ShutDown();
}

bool DataSender::Send(const base::StringPiece& data,
                      const DataSentCallback& callback,
                      const SendErrorCallback& error_callback) {
  DCHECK(!callback.is_null() && !error_callback.is_null());
  if (!pending_cancel_.is_null() || shut_down_)
    return false;

  pending_sends_.push(linked_ptr<PendingSend>(
      new PendingSend(data, callback, error_callback, fatal_error_value_)));
  SendInternal();
  return true;
}

bool DataSender::Cancel(int32_t error, const CancelCallback& callback) {
  DCHECK(!callback.is_null());
  if (!pending_cancel_.is_null() || shut_down_)
    return false;
  if (pending_sends_.empty() && sends_awaiting_ack_.empty()) {
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
    return true;
  }

  pending_cancel_ = callback;
  sink_->Cancel(error);
  return true;
}

void DataSender::ReportBytesSent(uint32_t bytes_sent) {
  if (shut_down_)
    return;

  available_buffer_capacity_ += bytes_sent;
  while (bytes_sent != 0 && !sends_awaiting_ack_.empty() &&
         sends_awaiting_ack_.front()->ReportBytesSent(&bytes_sent)) {
    sends_awaiting_ack_.pop();
  }
  if (bytes_sent > 0 && !pending_sends_.empty()) {
    bool finished = pending_sends_.front()->ReportBytesSent(&bytes_sent);
    DCHECK(!finished);
    if (finished) {
      ShutDown();
      return;
    }
  }
  if (bytes_sent != 0) {
    ShutDown();
    return;
  }
  if (pending_sends_.empty() && sends_awaiting_ack_.empty())
    RunCancelCallback();
  SendInternal();
}

void DataSender::ReportBytesSentAndError(
    uint32_t bytes_sent,
    int32_t error,
    const mojo::Callback<void()>& callback) {
  if (shut_down_)
    return;

  available_buffer_capacity_ += bytes_sent;
  while (!sends_awaiting_ack_.empty()) {
    available_buffer_capacity_ +=
        sends_awaiting_ack_.front()->ReportBytesSentAndError(&bytes_sent,
                                                             error);
    sends_awaiting_ack_.pop();
  }
  while (!pending_sends_.empty()) {
    available_buffer_capacity_ +=
        pending_sends_.front()->ReportBytesSentAndError(&bytes_sent, error);
    pending_sends_.pop();
  }
  callback.Run();
  RunCancelCallback();
}

void DataSender::OnConnectionError() {
  ShutDown();
}

void DataSender::SendInternal() {
  while (!pending_sends_.empty() && available_buffer_capacity_) {
    if (pending_sends_.front()->SendData(sink_.get(),
                                         &available_buffer_capacity_)) {
      sends_awaiting_ack_.push(pending_sends_.front());
      pending_sends_.pop();
    }
  }
}

void DataSender::RunCancelCallback() {
  DCHECK(pending_sends_.empty() && sends_awaiting_ack_.empty());
  if (pending_cancel_.is_null())
    return;

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(pending_cancel_));
  pending_cancel_.Reset();
}

void DataSender::ShutDown() {
  shut_down_ = true;
  while (!pending_sends_.empty()) {
    pending_sends_.front()->DispatchFatalError();
    pending_sends_.pop();
  }
  while (!sends_awaiting_ack_.empty()) {
    sends_awaiting_ack_.front()->DispatchFatalError();
    sends_awaiting_ack_.pop();
  }
  RunCancelCallback();
}

DataSender::PendingSend::PendingSend(const base::StringPiece& data,
                                     const DataSentCallback& callback,
                                     const SendErrorCallback& error_callback,
                                     int32_t fatal_error_value)
    : data_(data),
      callback_(callback),
      error_callback_(error_callback),
      fatal_error_value_(fatal_error_value),
      bytes_sent_(0),
      bytes_acked_(0) {
}

bool DataSender::PendingSend::ReportBytesSent(uint32_t* num_bytes) {
  ReportBytesSentInternal(num_bytes);
  if (bytes_acked_ < data_.size())
    return false;

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback_, bytes_acked_));
  return true;
}

uint32_t DataSender::PendingSend::ReportBytesSentAndError(uint32_t* num_bytes,
                                                          int32_t error) {
  ReportBytesSentInternal(num_bytes);
  if (*num_bytes > 0) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback_, bytes_acked_));
    return 0;
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(error_callback_, bytes_acked_, error));
  return bytes_sent_ - bytes_acked_;
}

void DataSender::PendingSend::DispatchFatalError() {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(error_callback_, 0, fatal_error_value_));
}

bool DataSender::PendingSend::SendData(serial::DataSink* sink,
                                       uint32_t* available_buffer_size) {
  uint32_t num_bytes_to_send =
      std::min(static_cast<uint32_t>(data_.size() - bytes_sent_),
               *available_buffer_size);
  mojo::Array<uint8_t> bytes(num_bytes_to_send);
  memcpy(&bytes[0], data_.data() + bytes_sent_, num_bytes_to_send);
  bytes_sent_ += num_bytes_to_send;
  *available_buffer_size -= num_bytes_to_send;
  sink->OnData(bytes.Pass());
  return bytes_sent_ == data_.size();
}

void DataSender::PendingSend::ReportBytesSentInternal(uint32_t* num_bytes) {
  bytes_acked_ += *num_bytes;
  if (bytes_acked_ > bytes_sent_) {
    *num_bytes = bytes_acked_ - bytes_sent_;
    bytes_acked_ = bytes_sent_;
  } else {
    *num_bytes = 0;
  }
}

}  // namespace device
