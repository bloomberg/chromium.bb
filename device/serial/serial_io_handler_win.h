// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_
#define DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/serial/serial_io_handler.h"

namespace device {

class SerialIoHandlerWin : public SerialIoHandler,
                           public base::MessageLoopForIO::IOHandler {
 protected:
  // SerialIoHandler implementation.
  virtual void ReadImpl() OVERRIDE;
  virtual void WriteImpl() OVERRIDE;
  virtual void CancelReadImpl() OVERRIDE;
  virtual void CancelWriteImpl() OVERRIDE;
  virtual bool Flush() const OVERRIDE;
  virtual serial::DeviceControlSignalsPtr GetControlSignals() const OVERRIDE;
  virtual bool SetControlSignals(
      const serial::HostControlSignals& control_signals) OVERRIDE;
  virtual bool ConfigurePort(const serial::ConnectionOptions& options) OVERRIDE;
  virtual serial::ConnectionInfoPtr GetPortInfo() const OVERRIDE;
  virtual bool PostOpen() OVERRIDE;

 private:
  friend class SerialIoHandler;

  explicit SerialIoHandlerWin(
      scoped_refptr<base::MessageLoopProxy> file_thread_message_loop);
  virtual ~SerialIoHandlerWin();

  // base::MessageLoopForIO::IOHandler implementation.
  virtual void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error) OVERRIDE;

  // Context used for asynchronous WaitCommEvent calls.
  scoped_ptr<base::MessageLoopForIO::IOContext> comm_context_;

  // Context used for overlapped reads.
  scoped_ptr<base::MessageLoopForIO::IOContext> read_context_;

  // Context used for overlapped writes.
  scoped_ptr<base::MessageLoopForIO::IOContext> write_context_;

  // Asynchronous event mask state
  DWORD event_mask_;

  // Indicates if a pending read is waiting on initial data arrival via
  // WaitCommEvent, as opposed to waiting on actual ReadFile completion
  // after a corresponding WaitCommEvent has completed.
  bool is_comm_pending_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandlerWin);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_
