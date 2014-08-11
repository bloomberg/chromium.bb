// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_SOURCE_SENDER_H_
#define DEVICE_SERIAL_DATA_SOURCE_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "device/serial/buffer.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

class AsyncWaiter;

// A DataSourceSender is an interface between a source of data and a data pipe.
class DataSourceSender : public base::RefCounted<DataSourceSender>,
                         public mojo::InterfaceImpl<serial::DataSource> {
 public:
  typedef base::Callback<void(scoped_ptr<WritableBuffer>)> ReadyCallback;
  typedef base::Callback<void()> ErrorCallback;

  // Constructs a DataSourceSender. Whenever the pipe is ready for writing, the
  // |ready_callback| will be called with the WritableBuffer to be filled.
  // |ready_callback| will not be called again until the previous WritableBuffer
  // is destroyed. If a connection error occurs, |error_callback| will be
  // called and the DataSourceSender will act as if ShutDown() had been called.
  DataSourceSender(const ReadyCallback& ready_callback,
                   const ErrorCallback& error_callback);

  // Shuts down this DataSourceSender. After shut down, |ready_callback| and
  // |error_callback| will never be called.
  void ShutDown();

 private:
  friend class base::RefCounted<DataSourceSender>;
  class PendingSend;

  virtual ~DataSourceSender();

  // mojo::InterfaceImpl<serial::DataSourceSender> overrides.
  virtual void Init(mojo::ScopedDataPipeProducerHandle handle) OVERRIDE;
  virtual void Resume() OVERRIDE;
  // Invoked in the event of a connection error. Calls DispatchFatalError().
  virtual void OnConnectionError() OVERRIDE;

  // Starts waiting for |handle_| to be ready for writes.
  void StartWaiting();

  // Invoked when |handle_| is ready for writes.
  void OnDoneWaiting(MojoResult result);

  // Reports a successful write of |bytes_written|.
  void Done(uint32_t bytes_written);

  // Reports a partially successful or unsuccessful write of |bytes_written|
  // with an error of |error|.
  void DoneWithError(uint32_t bytes_written, int32_t error);

  // Finishes the two-phase data pipe write.
  void DoneInternal(uint32_t bytes_written);

  // Reports a fatal error to the client and shuts down.
  void DispatchFatalError();

  // The data connection to the data receiver.
  mojo::ScopedDataPipeProducerHandle handle_;

  // The callback to call when |handle_| is ready for more data.
  ReadyCallback ready_callback_;

  // The callback to call if a fatal error occurs.
  ErrorCallback error_callback_;

  // The current pending send operation if there is one.
  scoped_ptr<PendingSend> pending_send_;

  // A waiter used to wait until |handle_| is writable if we are waiting.
  scoped_ptr<AsyncWaiter> waiter_;

  // The number of bytes sent to the data receiver.
  uint32_t bytes_sent_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  DISALLOW_COPY_AND_ASSIGN(DataSourceSender);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_SOURCE_SENDER_H_
