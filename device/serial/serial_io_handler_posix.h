// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
#define DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_

#include "base/message_loop/message_loop.h"
#include "device/serial/serial_io_handler.h"

namespace device {

class SerialIoHandlerPosix : public SerialIoHandler,
                             public base::MessageLoopForIO::Watcher {
 protected:
  // SerialIoHandler impl.
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
  void RequestAccess(
      const std::string& port,
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      scoped_refptr<base::MessageLoopProxy> ui_message_loop) override;

 private:
  friend class SerialIoHandler;

  SerialIoHandlerPosix(
      scoped_refptr<base::MessageLoopProxy> file_thread_message_loop,
      scoped_refptr<base::MessageLoopProxy> ui_thread_message_loop);
  ~SerialIoHandlerPosix() override;

  // base::MessageLoopForIO::Watcher implementation.
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  void EnsureWatchingReads();
  void EnsureWatchingWrites();

  base::MessageLoopForIO::FileDescriptorWatcher file_read_watcher_;
  base::MessageLoopForIO::FileDescriptorWatcher file_write_watcher_;

  // Flags indicating if the message loop is watching the device for IO events.
  bool is_watching_reads_;
  bool is_watching_writes_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandlerPosix);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
