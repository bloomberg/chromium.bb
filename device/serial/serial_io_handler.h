// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_IO_HANDLER_H_
#define DEVICE_SERIAL_SERIAL_IO_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/serial/buffer.h"
#include "device/serial/serial.mojom.h"

namespace device {

// Provides a simplified interface for performing asynchronous I/O on serial
// devices by hiding platform-specific MessageLoop interfaces. Pending I/O
// operations hold a reference to this object until completion so that memory
// doesn't disappear out from under the OS.
class SerialIoHandler : public base::RefCountedThreadSafe<SerialIoHandler> {
 public:
  // Constructs an instance of some platform-specific subclass.
  static scoped_refptr<SerialIoHandler> Create(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

  typedef base::Callback<void(bool success)> OpenCompleteCallback;

  // Initiates an asynchronous Open of the device.
  virtual void Open(const std::string& port,
                    const serial::ConnectionOptions& options,
                    const OpenCompleteCallback& callback);

#if defined(OS_CHROMEOS)
  // Signals that the port has been opened.
  void OnPathOpened(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      base::ScopedFD fd);

  // Signals that the permission broker failed to open the port.
  void OnPathOpenError(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      const std::string& error_name,
      const std::string& error_message);

  // Reports the open error from the permission broker.
  void ReportPathOpenError(const std::string& error_name,
                           const std::string& error_message);
#endif  // defined(OS_CHROMEOS)

  // Performs an async Read operation. Behavior is undefined if this is called
  // while a Read is already pending. Otherwise, the Done or DoneWithError
  // method on |buffer| will eventually be called with a result.
  void Read(std::unique_ptr<WritableBuffer> buffer);

  // Performs an async Write operation. Behavior is undefined if this is called
  // while a Write is already pending. Otherwise, the Done or DoneWithError
  // method on |buffer| will eventually be called with a result.
  void Write(std::unique_ptr<ReadOnlyBuffer> buffer);

  // Indicates whether or not a read is currently pending.
  bool IsReadPending() const;

  // Indicates whether or not a write is currently pending.
  bool IsWritePending() const;

  // Attempts to cancel a pending read operation.
  void CancelRead(serial::ReceiveError reason);

  // Attempts to cancel a pending write operation.
  void CancelWrite(serial::SendError reason);

  // Flushes input and output buffers.
  virtual bool Flush() const = 0;

  // Reads current control signals (DCD, CTS, etc.) into an existing
  // DeviceControlSignals structure. Returns |true| iff the signals were
  // successfully read.
  virtual serial::DeviceControlSignalsPtr GetControlSignals() const = 0;

  // Sets one or more control signals (DTR and/or RTS). Returns |true| iff
  // the signals were successfully set. Unininitialized flags in the
  // HostControlSignals structure are left unchanged.
  virtual bool SetControlSignals(
      const serial::HostControlSignals& control_signals) = 0;

  // Performs platform-specific port configuration. Returns |true| iff
  // configuration was successful.
  bool ConfigurePort(const serial::ConnectionOptions& options);

  // Performs a platform-specific port configuration query. Fills values in an
  // existing ConnectionInfo. Returns |true| iff port configuration was
  // successfully retrieved.
  virtual serial::ConnectionInfoPtr GetPortInfo() const = 0;

  // Initiates a BREAK signal. Places the transmission line in a break state
  // until the |ClearBreak| is called.
  virtual bool SetBreak() = 0;

  // Terminates the BREAK signal. Places the transmission line in a nonbreak
  // state.
  virtual bool ClearBreak() = 0;

 protected:
  explicit SerialIoHandler(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  virtual ~SerialIoHandler();

  // Performs a platform-specific read operation. This must guarantee that
  // ReadCompleted is called when the underlying async operation is completed
  // or the SerialIoHandler instance will leak.
  // NOTE: Implementations of ReadImpl should never call ReadCompleted directly.
  // Use QueueReadCompleted instead to avoid reentrancy.
  virtual void ReadImpl() = 0;

  // Performs a platform-specific write operation. This must guarantee that
  // WriteCompleted is called when the underlying async operation is completed
  // or the SerialIoHandler instance will leak.
  // NOTE: Implementations of WriteImpl should never call WriteCompleted
  // directly. Use QueueWriteCompleted instead to avoid reentrancy.
  virtual void WriteImpl() = 0;

  // Platform-specific read cancelation.
  virtual void CancelReadImpl() = 0;

  // Platform-specific write cancelation.
  virtual void CancelWriteImpl() = 0;

  // Platform-specific port configuration applies options_ to the device.
  virtual bool ConfigurePortImpl() = 0;

  // Performs platform-specific, one-time port configuration on open.
  virtual bool PostOpen();

  // Called by the implementation to signal that the active read has completed.
  // WARNING: Calling this method can destroy the SerialIoHandler instance
  // if the associated I/O operation was the only thing keeping it alive.
  void ReadCompleted(int bytes_read, serial::ReceiveError error);

  // Called by the implementation to signal that the active write has completed.
  // WARNING: Calling this method may destroy the SerialIoHandler instance
  // if the associated I/O operation was the only thing keeping it alive.
  void WriteCompleted(int bytes_written, serial::SendError error);

  // Queues a ReadCompleted call on the current thread. This is used to allow
  // ReadImpl to immediately signal completion with 0 bytes and an error,
  // without being reentrant.
  void QueueReadCompleted(int bytes_read, serial::ReceiveError error);

  // Queues a WriteCompleted call on the current thread. This is used to allow
  // WriteImpl to immediately signal completion with 0 bytes and an error,
  // without being reentrant.
  void QueueWriteCompleted(int bytes_written, serial::SendError error);

  const base::File& file() const { return file_; }

  char* pending_read_buffer() const {
    return pending_read_buffer_ ? pending_read_buffer_->GetData() : NULL;
  }

  uint32_t pending_read_buffer_len() const {
    return pending_read_buffer_ ? pending_read_buffer_->GetSize() : 0;
  }

  serial::ReceiveError read_cancel_reason() const {
    return read_cancel_reason_;
  }

  bool read_canceled() const { return read_canceled_; }

  const char* pending_write_buffer() const {
    return pending_write_buffer_ ? pending_write_buffer_->GetData() : NULL;
  }

  uint32_t pending_write_buffer_len() const {
    return pending_write_buffer_ ? pending_write_buffer_->GetSize() : 0;
  }

  serial::SendError write_cancel_reason() const { return write_cancel_reason_; }

  bool write_canceled() const { return write_canceled_; }

  const serial::ConnectionOptions& options() const { return options_; }

  // Possibly fixes up a serial port path name in a platform-specific manner.
  static std::string MaybeFixUpPortName(const std::string& port_name);

  base::SingleThreadTaskRunner* ui_thread_task_runner() const {
    return ui_thread_task_runner_.get();
  }

  const std::string& port() const { return port_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  friend class base::RefCountedThreadSafe<SerialIoHandler>;

  void MergeConnectionOptions(const serial::ConnectionOptions& options);

  // Continues an Open operation on the FILE thread.
  void StartOpen(const std::string& port,
                 scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Finalizes an Open operation (continued from StartOpen) on the IO thread.
  void FinishOpen(base::File file);

  void Close();

  // Continues a Close operation on the FILE thread.
  static void DoClose(base::File port);

  // File for the opened serial device. This value is only modified from the IO
  // thread.
  base::File file_;

  // Currently applied connection options.
  serial::ConnectionOptions options_;

  std::unique_ptr<WritableBuffer> pending_read_buffer_;
  serial::ReceiveError read_cancel_reason_;
  bool read_canceled_;

  std::unique_ptr<ReadOnlyBuffer> pending_write_buffer_;
  serial::SendError write_cancel_reason_;
  bool write_canceled_;

  // Callback to handle the completion of a pending Open() request.
  OpenCompleteCallback open_complete_;

  // On Chrome OS, PermissionBrokerClient should be called on the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  std::string port_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandler);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_IO_HANDLER_H_
