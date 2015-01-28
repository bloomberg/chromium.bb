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
  virtual void ReadImpl() override;
  virtual void WriteImpl() override;
  virtual void CancelReadImpl() override;
  virtual void CancelWriteImpl() override;
  virtual bool ConfigurePortImpl() override;
  virtual bool Flush() const override;
  virtual serial::DeviceControlSignalsPtr GetControlSignals() const override;
  virtual bool SetControlSignals(
      const serial::HostControlSignals& control_signals) override;
  virtual serial::ConnectionInfoPtr GetPortInfo() const override;
  virtual bool PostOpen() override;

 private:
  friend class SerialIoHandler;

  explicit SerialIoHandlerWin(
      scoped_refptr<base::MessageLoopProxy> file_thread_message_loop,
      scoped_refptr<base::MessageLoopProxy> ui_thread_message_loop);
  virtual ~SerialIoHandlerWin();

  // base::MessageLoopForIO::IOHandler implementation.
  virtual void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error) override;

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
