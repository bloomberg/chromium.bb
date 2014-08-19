// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_SENDER_H_
#define DEVICE_SERIAL_DATA_SENDER_H_

#include <queue>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string_piece.h"
#include "device/serial/buffer.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

class AsyncWaiter;

// A DataSender sends data to a DataSink.
class DataSender : public serial::DataSinkClient, public mojo::ErrorHandler {
 public:
  typedef base::Callback<void(uint32_t bytes_sent)> DataSentCallback;
  typedef base::Callback<void(uint32_t bytes_sent, int32_t error)>
      SendErrorCallback;
  typedef base::Callback<void()> CancelCallback;

  // Constructs a DataSender to send data to |sink|, using a data pipe with a
  // buffer size of |buffer_size|, with connection errors reported as
  // |fatal_error_value|.
  DataSender(mojo::InterfacePtr<serial::DataSink> sink,
             uint32_t buffer_size,
             int32_t fatal_error_value);

  virtual ~DataSender();

  // Starts an asynchronous send of |data|. If the send completes successfully,
  // |callback| will be called. Otherwise, |error_callback| will be called with
  // the error. If there is a cancel in progress or there has been a connection
  // error, this will not start a send and returns false. It is the caller's
  // responsibility to ensure |data| remains valid until one of |callback| and
  // |error_callback| is called.
  bool Send(const base::StringPiece& data,
            const DataSentCallback& callback,
            const SendErrorCallback& error_callback);

  // Requests the cancellation of any in-progress sends. Calls to Send() will
  // fail until |callback| is called.
  bool Cancel(int32_t error, const CancelCallback& callback);

 private:
  class PendingSend;

  // serial::DataSinkClient overrides.
  virtual void ReportBytesSent(uint32_t bytes_sent) OVERRIDE;
  virtual void ReportBytesSentAndError(
      uint32_t bytes_sent,
      int32_t error,
      const mojo::Callback<void(uint32_t)>& callback) OVERRIDE;

  // mojo::ErrorHandler override.
  virtual void OnConnectionError() OVERRIDE;

  // Copies data from |pending_sends_| into the data pipe and starts |waiter_|
  // waiting if the pipe is full. When a PendingSend in |pending_sends_| has
  // been fully copied into the data pipe, it moves to |sends_awaiting_ack_|.
  void SendInternal();

  // Invoked when |handle_| is ready for writes. Calls SendInternal().
  void OnDoneWaiting(MojoResult result);

  // Dispatches a cancel callback if one is pending.
  void RunCancelCallback();

  // Shuts down this DataSender and dispatches fatal errors to all pending
  // operations.
  void ShutDown();

  // The control connection to the data sink.
  mojo::InterfacePtr<serial::DataSink> sink_;

  // The data connection to the data sink.
  mojo::ScopedDataPipeProducerHandle handle_;

  // The error value to report in the event of a fatal error.
  const int32_t fatal_error_value_;

  // A waiter used to wait until |handle_| is writable if we are waiting.
  scoped_ptr<AsyncWaiter> waiter_;

  // A queue of PendingSend that have not yet been fully written to the data
  // pipe.
  std::queue<linked_ptr<PendingSend> > pending_sends_;

  // A queue of PendingSend that have been written to the data pipe, but have
  // not yet been acked by the DataSink.
  std::queue<linked_ptr<PendingSend> > sends_awaiting_ack_;

  // The callback to report cancel completion if a cancel operation is in
  // progress.
  CancelCallback pending_cancel_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  DISALLOW_COPY_AND_ASSIGN(DataSender);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_SENDER_H_
