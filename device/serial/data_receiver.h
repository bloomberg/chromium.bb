// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_RECEIVER_H_
#define DEVICE_SERIAL_DATA_RECEIVER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/serial/buffer.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

class AsyncWaiter;

// A DataReceiver receives data from a DataSource.
class DataReceiver : public base::RefCounted<DataReceiver>,
                     public serial::DataSourceClient,
                     public mojo::ErrorHandler {
 public:
  typedef base::Callback<void(scoped_ptr<ReadOnlyBuffer>)> ReceiveDataCallback;
  typedef base::Callback<void(int32_t error)> ReceiveErrorCallback;

  // Constructs a DataReceiver to receive data from |source|, using a data
  // pipe with a buffer size of |buffer_size|, with connection errors reported
  // as |fatal_error_value|.
  DataReceiver(mojo::InterfacePtr<serial::DataSource> source,
               uint32_t buffer_size,
               int32_t fatal_error_value);

  // Begins a receive operation. If this returns true, exactly one of |callback|
  // and |error_callback| will eventually be called. If there is already a
  // receive in progress or there has been a connection error, this will have no
  // effect and return false.
  bool Receive(const ReceiveDataCallback& callback,
               const ReceiveErrorCallback& error_callback);

 private:
  class PendingReceive;
  struct PendingError;
  friend class base::RefCounted<DataReceiver>;

  virtual ~DataReceiver();

  // serial::DataSourceClient override. Invoked by the DataSource to report
  // errors.
  virtual void OnError(uint32_t bytes_since_last_error, int32_t error) OVERRIDE;

  // mojo::ErrorHandler override. Calls ShutDown().
  virtual void OnConnectionError() OVERRIDE;

  // Invoked by the PendingReceive to report that the user is done with the
  // receive buffer, having read |bytes_read| bytes from it.
  void Done(uint32_t bytes_read);

  // Invoked when |handle_| is ready to read. Unless an error has occurred, this
  // calls ReceiveInternal().
  void OnDoneWaiting(MojoResult result);

  // The implementation of Receive(). If a |pending_error_| is ready to
  // dispatch, it does so. Otherwise, this attempts to read from |handle_| and
  // dispatches the contents to |pending_receive_|. If |handle_| is not ready,
  // |waiter_| is used to wait until it is ready. If an error occurs in reading
  // |handle_|, ShutDown() is called.
  void ReceiveInternal();

  // We may have been notified of an error that occurred at some future point in
  // the stream. We should never be able to read past the point at which the
  // error occurred until we have dealt with the error and called Resume() on
  // the DataSource. If this has occurred, something bad has happened on the
  // service side, so we shut down.
  bool CheckErrorNotInReadRange(uint32_t num_bytes);

  // Called when we encounter a fatal error. If a receive is in progress,
  // |fatal_error_value_| will be reported to the user.
  void ShutDown();

  // The control connection to the data source.
  mojo::InterfacePtr<serial::DataSource> source_;

  // The data connection to the data source.
  mojo::ScopedDataPipeConsumerHandle handle_;

  // The error value to report in the event of a fatal error.
  const int32_t fatal_error_value_;

  // The number of bytes received from the data source.
  uint32_t bytes_received_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  // A waiter used to wait until |handle_| is readable if we are waiting.
  scoped_ptr<AsyncWaiter> waiter_;

  // The current pending receive operation if there is one.
  scoped_ptr<PendingReceive> pending_receive_;

  // The current pending error if there is one.
  scoped_ptr<PendingError> pending_error_;

  base::WeakPtrFactory<DataReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReceiver);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_RECEIVER_H_
