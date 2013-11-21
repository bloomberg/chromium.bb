// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_IO_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_IO_HANDLER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/common/extensions/api/serial.h"
#include "net/base/io_buffer.h"

namespace extensions {

// Provides a simplified interface for performing asynchronous I/O on serial
// devices by hiding platform-specific MessageLoop interfaces. Pending I/O
// operations hold a reference to this object until completion so that memory
// doesn't disappear out from under the OS.
class SerialIoHandler : public base::NonThreadSafe,
                        public base::RefCounted<SerialIoHandler> {
 public:
  // Constructs an instance of some platform-specific subclass.
  static scoped_refptr<SerialIoHandler> Create();

  // Called with a string of bytes read, and a result code. Note that an error
  // result does not necessarily imply 0 bytes read.
  typedef base::Callback<void(const std::string& data,
                              api::serial::ReceiveError error)>
      ReadCompleteCallback;

  // Called with the number of bytes written and a result code. Note that an
  // error result does not necessarily imply 0 bytes written.
  typedef base::Callback<void(int bytes_written, api::serial::SendError error)>
      WriteCompleteCallback;

  // Initializes the handler on the current message loop. Must be called exactly
  // once before performing any I/O through the handler.
  void Initialize(base::PlatformFile file,
                  const ReadCompleteCallback& read_callback,
                  const WriteCompleteCallback& write_callback);

  // Performs an async Read operation. Behavior is undefined if this is called
  // while a Read is already pending. Otherwise, the ReadCompleteCallback
  // (see above) will eventually be called with a result.
  void Read(int max_bytes);

  // Performs an async Write operation. Behavior is undefined if this is called
  // while a Write is already pending. Otherwise, the WriteCompleteCallback
  // (see above) will eventually be called with a result.
  void Write(const std::string& data);

  // Indicates whether or not a read is currently pending.
  bool IsReadPending() const;

  // Indicates whether or not a write is currently pending.
  bool IsWritePending() const;

  // Attempts to cancel a pending read operation.
  void CancelRead(api::serial::ReceiveError reason);

  // Attempts to cancel a pending write operation.
  void CancelWrite(api::serial::SendError reason);

 protected:
  SerialIoHandler();
  virtual ~SerialIoHandler();

  // Performs platform-specific initialization. |file_|, |read_complete_| and
  // |write_complete_| all hold initialized values before this is called.
  virtual void InitializeImpl() {}

  // Performs a platform-specific read operation. This must guarantee that
  // ReadCompleted is called when the underlying async operation is completed
  // or the SerialIoHandler instance will leak.
  // NOTE: Implementations of ReadImpl should never call ReadCompleted directly.
  // Use QueueReadCompleted instead to avoid reentrancy.
  virtual void ReadImpl() = 0;

  // Performs a platform-specific write operation. This must guarantee that
  // WriteCompleted is called when the underlying async operation is completed
  // or the SerialIoHandler instance will leak.
  // NOTE: Implementations of Writempl should never call WriteCompleted
  // directly. Use QueueWriteCompleted instead to avoid reentrancy.
  virtual void WriteImpl() = 0;

  // Platform-specific read cancelation.
  virtual void CancelReadImpl() = 0;

  // Platform-specific write cancelation.
  virtual void CancelWriteImpl() = 0;

  // Called by the implementation to signal that the active read has completed.
  // WARNING: Calling this method can destroy the SerialIoHandler instance
  // if the associated I/O operation was the only thing keeping it alive.
  void ReadCompleted(int bytes_read, api::serial::ReceiveError error);

  // Called by the implementation to signal that the active write has completed.
  // WARNING: Calling this method may destroy the SerialIoHandler instance
  // if the associated I/O operation was the only thing keeping it alive.
  void WriteCompleted(int bytes_written, api::serial::SendError error);

  // Queues a ReadCompleted call on the current thread. This is used to allow
  // ReadImpl to immediately signal completion with 0 bytes and an error,
  // without being reentrant.
  void QueueReadCompleted(int bytes_read, api::serial::ReceiveError error);

  // Queues a WriteCompleted call on the current thread. This is used to allow
  // WriteImpl to immediately signal completion with 0 bytes and an error,
  // without being reentrant.
  void QueueWriteCompleted(int bytes_written, api::serial::SendError error);

  base::PlatformFile file() const {
    return file_;
  }

  net::IOBuffer* pending_read_buffer() const {
    return pending_read_buffer_.get();
  }

  int pending_read_buffer_len() const {
    return pending_read_buffer_len_;
  }

  api::serial::ReceiveError read_cancel_reason() const {
    return read_cancel_reason_;
  }

  bool read_canceled() const {
    return read_canceled_;
  }

  net::IOBuffer* pending_write_buffer() const {
    return pending_write_buffer_.get();
  }

  int pending_write_buffer_len() const {
    return pending_write_buffer_len_;
  }

  api::serial::SendError write_cancel_reason() const {
    return write_cancel_reason_;
  }

  bool write_canceled() const {
    return write_canceled_;
  }

 private:
  friend class base::RefCounted<SerialIoHandler>;

  base::PlatformFile file_;

  scoped_refptr<net::IOBuffer> pending_read_buffer_;
  int pending_read_buffer_len_;
  api::serial::ReceiveError read_cancel_reason_;
  bool read_canceled_;

  scoped_refptr<net::IOBuffer> pending_write_buffer_;
  int pending_write_buffer_len_;
  api::serial::SendError write_cancel_reason_;
  bool write_canceled_;

  ReadCompleteCallback read_complete_;
  WriteCompleteCallback write_complete_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_IO_HANDLER_H_
