// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_sender.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "device/serial/async_waiter.h"

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

  // Attempts to send any data not yet sent to |handle|. Returns MOJO_RESULT_OK
  // if all data is sent, MOJO_RESULT_SHOULD_WAIT if not all of the data is sent
  // or the error if one is encountered writing to |handle|.
  MojoResult SendData(mojo::DataPipeProducerHandle handle);

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

  // The number of bytes sent to the data pipe.
  uint32_t bytes_sent_;

  // The number of bytes acked.
  uint32_t bytes_acked_;
};

DataSender::DataSender(mojo::InterfacePtr<serial::DataSink> sink,
                       uint32_t buffer_size,
                       int32_t fatal_error_value)
    : sink_(sink.Pass()),
      fatal_error_value_(fatal_error_value),
      shut_down_(false) {
  sink_.set_error_handler(this);
  MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE, 1, buffer_size,
  };
  options.struct_size = sizeof(options);
  mojo::ScopedDataPipeConsumerHandle remote_handle;
  MojoResult result = mojo::CreateDataPipe(&options, &handle_, &remote_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  sink_->Init(remote_handle.Pass());
  sink_.set_client(this);
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
}

void DataSender::ReportBytesSentAndError(
    uint32_t bytes_sent,
    int32_t error,
    const mojo::Callback<void(uint32_t)>& callback) {
  if (shut_down_)
    return;

  uint32_t bytes_to_flush = 0;
  while (!sends_awaiting_ack_.empty()) {
    bytes_to_flush += sends_awaiting_ack_.front()->ReportBytesSentAndError(
        &bytes_sent, error);
    sends_awaiting_ack_.pop();
  }
  while (!pending_sends_.empty()) {
    bytes_to_flush +=
        pending_sends_.front()->ReportBytesSentAndError(&bytes_sent, error);
    pending_sends_.pop();
  }
  callback.Run(bytes_to_flush);
  RunCancelCallback();
}

void DataSender::OnConnectionError() {
  ShutDown();
}

void DataSender::SendInternal() {
  while (!pending_sends_.empty()) {
    MojoResult result = pending_sends_.front()->SendData(handle_.get());
    if (result == MOJO_RESULT_OK) {
      sends_awaiting_ack_.push(pending_sends_.front());
      pending_sends_.pop();
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      waiter_.reset(new AsyncWaiter(
          handle_.get(),
          MOJO_HANDLE_SIGNAL_WRITABLE,
          base::Bind(&DataSender::OnDoneWaiting, base::Unretained(this))));
      return;
    } else {
      ShutDown();
      return;
    }
  }
}

void DataSender::OnDoneWaiting(MojoResult result) {
  waiter_.reset();
  if (result != MOJO_RESULT_OK) {
    ShutDown();
    return;
  }
  SendInternal();
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
  waiter_.reset();
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

MojoResult DataSender::PendingSend::SendData(
    mojo::DataPipeProducerHandle handle) {
  uint32_t bytes_to_send = static_cast<uint32_t>(data_.size()) - bytes_sent_;
  MojoResult result = mojo::WriteDataRaw(handle,
                                         data_.data() + bytes_sent_,
                                         &bytes_to_send,
                                         MOJO_WRITE_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return result;

  bytes_sent_ += bytes_to_send;
  return bytes_sent_ == data_.size() ? MOJO_RESULT_OK : MOJO_RESULT_SHOULD_WAIT;
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
