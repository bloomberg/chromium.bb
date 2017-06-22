// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_
#define DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/serial/serial_io_handler.h"

namespace device {

class SerialIoHandlerWin : public SerialIoHandler,
                           public base::MessageLoopForIO::IOHandler {
 protected:
  // SerialIoHandler implementation.
  void ReadImpl() override;
  void WriteImpl() override;
  void CancelReadImpl() override;
  void CancelWriteImpl() override;
  bool ConfigurePortImpl() override;
  bool Flush() const override;
  serial::DeviceControlSignalsPtr GetControlSignals() const override;
  bool SetControlSignals(
      const serial::HostControlSignals& control_signals) override;
  serial::ConnectionInfoPtr GetPortInfo() const override;
  bool SetBreak() override;
  bool ClearBreak() override;
  bool PostOpen() override;

 private:
  class UiThreadHelper;
  friend class SerialIoHandler;

  explicit SerialIoHandlerWin(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  ~SerialIoHandlerWin() override;

  // base::MessageLoopForIO::IOHandler implementation.
  void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

  void OnDeviceRemoved(const std::string& device_path);

  // Context used for asynchronous WaitCommEvent calls.
  std::unique_ptr<base::MessageLoopForIO::IOContext> comm_context_;

  // Context used for overlapped reads.
  std::unique_ptr<base::MessageLoopForIO::IOContext> read_context_;

  // Context used for overlapped writes.
  std::unique_ptr<base::MessageLoopForIO::IOContext> write_context_;

  // Asynchronous event mask state
  DWORD event_mask_;

  // Indicates if a pending read is waiting on initial data arrival via
  // WaitCommEvent, as opposed to waiting on actual ReadFile completion
  // after a corresponding WaitCommEvent has completed.
  bool is_comm_pending_;

  // The helper lives on the UI thread and holds a weak reference back to the
  // handler that owns it.
  UiThreadHelper* helper_;
  base::WeakPtrFactory<SerialIoHandlerWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandlerWin);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_
