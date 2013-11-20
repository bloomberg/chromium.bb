// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_IO_HANDLER_POSIX_H_

#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/serial/serial_io_handler.h"

namespace extensions {

class SerialIoHandlerPosix : public SerialIoHandler,
                             public base::MessageLoopForIO::Watcher {
 protected:
  // SerialIoHandler impl.
  virtual void ReadImpl() OVERRIDE;
  virtual void WriteImpl() OVERRIDE;
  virtual void CancelReadImpl() OVERRIDE;
  virtual void CancelWriteImpl() OVERRIDE;

 private:
  friend class SerialIoHandler;

  SerialIoHandlerPosix();
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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_IO_HANDLER_POSIX_H_

