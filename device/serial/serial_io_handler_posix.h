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

  SerialIoHandlerPosix(
      scoped_refptr<base::MessageLoopProxy> file_thread_message_loop);
  virtual ~SerialIoHandlerPosix();

  // base::MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;

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
