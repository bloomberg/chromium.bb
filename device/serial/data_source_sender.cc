// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_source_sender.h"

#include <limits>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "device/serial/async_waiter.h"

namespace device {

// Represents a send that is not yet fulfilled.
class DataSourceSender::PendingSend {
 public:
  PendingSend(DataSourceSender* sender, const ReadyCallback& callback);

  // Asynchronously fills |data| with up to |num_bytes| of data. Following this,
  // one of Done() and DoneWithError() will be called with the result.
  void GetData(void* data, uint32_t num_bytes);

 private:
  class Buffer;
  // Reports a successful write of |bytes_written|.
  void Done(uint32_t bytes_written);

  // Reports a partially successful or unsuccessful write of |bytes_written|
  // with an error of |error|.
  void DoneWithError(uint32_t bytes_written, int32_t error);

  // The DataSourceSender that owns this.
  DataSourceSender* sender_;

  // The callback to call to get data.
  ReadyCallback callback_;

  // Whether the buffer specified by GetData() has been passed to |callback_|,
  // but has not yet called Done() or DoneWithError().
  bool buffer_in_use_;
};

// A Writable implementation that provides a view of a data pipe owned by a
// DataSourceSender.
class DataSourceSender::PendingSend::Buffer : public WritableBuffer {
 public:
  Buffer(scoped_refptr<DataSourceSender> sender,
         PendingSend* send,
         char* buffer,
         uint32_t buffer_size);
  virtual ~Buffer();

  // WritableBuffer overrides.
  virtual char* GetData() OVERRIDE;
  virtual uint32_t GetSize() OVERRIDE;
  virtual void Done(uint32_t bytes_written) OVERRIDE;
  virtual void DoneWithError(uint32_t bytes_written, int32_t error) OVERRIDE;

 private:
  // The DataSourceSender whose data pipe we are providing a view.
  scoped_refptr<DataSourceSender> sender_;

  // The PendingSend to which this buffer has been created in response.
  PendingSend* pending_send_;

  char* buffer_;
  uint32_t buffer_size_;
};

DataSourceSender::DataSourceSender(const ReadyCallback& ready_callback,
                                   const ErrorCallback& error_callback)
    : ready_callback_(ready_callback),
      error_callback_(error_callback),
      bytes_sent_(0),
      shut_down_(false) {
  DCHECK(!ready_callback.is_null() && !error_callback.is_null());
}

void DataSourceSender::ShutDown() {
  shut_down_ = true;
  waiter_.reset();
  ready_callback_.Reset();
  error_callback_.Reset();
}

DataSourceSender::~DataSourceSender() {
  DCHECK(shut_down_);
}

void DataSourceSender::Init(mojo::ScopedDataPipeProducerHandle handle) {
  // This should never occur. |handle_| is only valid and |pending_send_| is
  // only set after Init is called.
  if (pending_send_ || handle_.is_valid() || shut_down_) {
    DispatchFatalError();
    return;
  }
  handle_ = handle.Pass();
  pending_send_.reset(new PendingSend(this, ready_callback_));
  StartWaiting();
}

void DataSourceSender::Resume() {
  if (pending_send_ || !handle_.is_valid()) {
    DispatchFatalError();
    return;
  }

  pending_send_.reset(new PendingSend(this, ready_callback_));
  StartWaiting();
}

void DataSourceSender::OnConnectionError() {
  DispatchFatalError();
}

void DataSourceSender::StartWaiting() {
  DCHECK(pending_send_ && !waiter_);
  waiter_.reset(
      new AsyncWaiter(handle_.get(),
                      MOJO_HANDLE_SIGNAL_WRITABLE,
                      base::Bind(&DataSourceSender::OnDoneWaiting, this)));
}

void DataSourceSender::OnDoneWaiting(MojoResult result) {
  DCHECK(pending_send_ && !shut_down_ && waiter_);
  waiter_.reset();
  if (result != MOJO_RESULT_OK) {
    DispatchFatalError();
    return;
  }
  void* data = NULL;
  uint32_t num_bytes = std::numeric_limits<uint32_t>::max();
  result = mojo::BeginWriteDataRaw(
      handle_.get(), &data, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    DispatchFatalError();
    return;
  }
  pending_send_->GetData(static_cast<char*>(data), num_bytes);
}

void DataSourceSender::Done(uint32_t bytes_written) {
  DoneInternal(bytes_written);
  if (!shut_down_)
    StartWaiting();
}

void DataSourceSender::DoneWithError(uint32_t bytes_written, int32_t error) {
  DoneInternal(bytes_written);
  pending_send_.reset();
  if (!shut_down_)
    client()->OnError(bytes_sent_, error);
  // We don't call StartWaiting here so we don't send any additional data until
  // Resume() is called.
}

void DataSourceSender::DoneInternal(uint32_t bytes_written) {
  DCHECK(pending_send_);
  if (shut_down_)
    return;

  bytes_sent_ += bytes_written;
  MojoResult result = mojo::EndWriteDataRaw(handle_.get(), bytes_written);
  if (result != MOJO_RESULT_OK) {
    DispatchFatalError();
    return;
  }
}

void DataSourceSender::DispatchFatalError() {
  if (shut_down_)
    return;

  error_callback_.Run();
  ShutDown();
}

DataSourceSender::PendingSend::PendingSend(DataSourceSender* sender,
                                           const ReadyCallback& callback)
    : sender_(sender), callback_(callback), buffer_in_use_(false) {
}

void DataSourceSender::PendingSend::GetData(void* data, uint32_t num_bytes) {
  DCHECK(!buffer_in_use_);
  buffer_in_use_ = true;
  callback_.Run(scoped_ptr<WritableBuffer>(
      new Buffer(sender_, this, static_cast<char*>(data), num_bytes)));
}

void DataSourceSender::PendingSend::Done(uint32_t bytes_written) {
  DCHECK(buffer_in_use_);
  buffer_in_use_ = false;
  sender_->Done(bytes_written);
}

void DataSourceSender::PendingSend::DoneWithError(uint32_t bytes_written,
                                                  int32_t error) {
  DCHECK(buffer_in_use_);
  buffer_in_use_ = false;
  sender_->DoneWithError(bytes_written, error);
}

DataSourceSender::PendingSend::Buffer::Buffer(
    scoped_refptr<DataSourceSender> sender,
    PendingSend* send,
    char* buffer,
    uint32_t buffer_size)
    : sender_(sender),
      pending_send_(send),
      buffer_(buffer),
      buffer_size_(buffer_size) {
}

DataSourceSender::PendingSend::Buffer::~Buffer() {
  if (sender_.get())
    pending_send_->Done(0);
}

char* DataSourceSender::PendingSend::Buffer::GetData() {
  return buffer_;
}

uint32_t DataSourceSender::PendingSend::Buffer::GetSize() {
  return buffer_size_;
}

void DataSourceSender::PendingSend::Buffer::Done(uint32_t bytes_written) {
  DCHECK(sender_.get());
  pending_send_->Done(bytes_written);
  sender_ = NULL;
  pending_send_ = NULL;
  buffer_ = NULL;
  buffer_size_ = 0;
}

void DataSourceSender::PendingSend::Buffer::DoneWithError(
    uint32_t bytes_written,
    int32_t error) {
  DCHECK(sender_.get());
  pending_send_->DoneWithError(bytes_written, error);
  sender_ = NULL;
  pending_send_ = NULL;
  buffer_ = NULL;
  buffer_size_ = 0;
}

}  // namespace device
