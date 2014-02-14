// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/platform_file.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/serial/serial_io_handler.h"
#include "chrome/common/extensions/api/serial.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"

using content::BrowserThread;

namespace extensions {

// Encapsulates an open serial port. Platform-specific implementations are in
// _win and _posix versions of the the .cc file.
// NOTE: Instances of this object should only be constructed on the IO thread,
// and all methods should only be called on the IO thread unless otherwise
// noted.
class SerialConnection : public ApiResource,
                         public base::SupportsWeakPtr<SerialConnection> {
 public:
  typedef base::Callback<void(bool success)> OpenCompleteCallback;

  // This is the callback type expected by Receive. Note that an error result
  // does not necessarily imply an empty |data| string, since a receive may
  // complete partially before being interrupted by an error condition.
  typedef base::Callback<void(const std::string& data,
                              api::serial::ReceiveError error)>
      ReceiveCompleteCallback;

  // This is the callback type expected by Send. Note that an error result
  // does not necessarily imply 0 bytes sent, since a send may complete
  // partially before being interrupted by an error condition.
  typedef base::Callback<void(int bytes_sent,
                              api::serial::SendError error)>
      SendCompleteCallback;

  SerialConnection(const std::string& port,
                   const std::string& owner_extension_id);
  virtual ~SerialConnection();

  // ApiResource override.
  virtual bool IsPersistent() const OVERRIDE;

  void set_persistent(bool persistent) { persistent_ = persistent; }
  bool persistent() const { return persistent_; }

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_buffer_size(int buffer_size);
  int buffer_size() const { return buffer_size_; }

  void set_receive_timeout(int receive_timeout);
  int receive_timeout() const { return receive_timeout_; }

  void set_send_timeout(int send_timeout);
  int send_timeout() const { return send_timeout_; }

  void set_paused(bool paused);
  bool paused() const { return paused_; }

  // Initiates an asynchronous Open of the device. It is the caller's
  // responsibility to ensure that this SerialConnection stays alive
  // until |callback| is run.
  virtual void Open(const OpenCompleteCallback& callback);

  // Initiate a Close of the device. The SerialConnection instance will
  // have its internal state reset synchronously upon calling this, but
  // the underlying OS handle will be closed asynchronously.
  virtual void Close();

  // Begins an asynchronous receive operation. Calling this while a Receive
  // is already pending is a no-op and returns |false| without calling
  // |callback|.
  virtual bool Receive(const ReceiveCompleteCallback& callback);

  // Begins an asynchronous send operation. Calling this while a Send
  // is already pending is a no-op and returns |false| without calling
  // |callback|.
  virtual bool Send(const std::string& data,
                    const SendCompleteCallback& callback);

  // Flushes input and output buffers.
  virtual bool Flush() const;

  // Configures some subset of port options for this connection.
  // Omitted options are unchanged. Returns |true| iff the configuration
  // changes were successful.
  virtual bool Configure(const api::serial::ConnectionOptions& options);

  // Connection configuration query. Fills values in an existing
  // ConnectionInfo. Returns |true| iff the connection's information
  // was successfully retrieved.
  virtual bool GetInfo(api::serial::ConnectionInfo* info) const;

  // Reads current control signals (DCD, CTS, etc.) into an existing
  // DeviceControlSignals structure. Returns |true| iff the signals were
  // successfully read.
  virtual bool GetControlSignals(api::serial::DeviceControlSignals*
                                 control_signals) const;

  // Sets one or more control signals (DTR and/or RTS). Returns |true| iff
  // the signals were successfully set. Unininitialized flags in the
  // HostControlSignals structure are left unchanged.
  virtual bool SetControlSignals(
      const api::serial::HostControlSignals& control_signals);

  static const BrowserThread::ID kThreadId = BrowserThread::IO;

 protected:
  // Overrides |io_handler_| for testing.
  virtual void SetIoHandlerForTest(scoped_refptr<SerialIoHandler> handler);

  // Performs platform-specific, one-time port configuration on open.
  bool PostOpen();

  // Performs platform-specific port configuration. Returns |true| iff
  // configuration was successful.
  bool ConfigurePort(const api::serial::ConnectionOptions& options);

  // Performs a platform-specific port configuration query. Fills values in an
  // existing ConnectionInfo. Returns |true| iff port configuration was
  // successfully retrieved.
  bool GetPortInfo(api::serial::ConnectionInfo* info) const;

  // Possibly fixes up a serial port path name in a platform-specific manner.
  static std::string MaybeFixUpPortName(const std::string &port_name);

 private:
  friend class ApiResourceManager<SerialConnection>;
  static const char* service_name() {
    return "SerialConnectionManager";
  }

  // Encapsulates a cancelable, delayed timeout task. Posts a delayed
  // task upon construction and implicitly cancels the task upon
  // destruction if it hasn't run yet.
  class TimeoutTask {
   public:
    TimeoutTask(const base::Closure& closure, const base::TimeDelta& delay);
    ~TimeoutTask();

   private:
    void Run() const;

    base::WeakPtrFactory<TimeoutTask> weak_factory_;
    base::Closure closure_;
    base::TimeDelta delay_;
  };

  // Continues an Open operation on the FILE thread.
  void StartOpen();

  // Finalizes an Open operation (continued from StartOpen) on the IO thread.
  void FinishOpen(base::PlatformFile file);

  // Continues a Close operation on the FILE thread.
  static void DoClose(base::PlatformFile port);

  // Handles a receive timeout.
  void OnReceiveTimeout();

  // Handles a send timeout.
  void OnSendTimeout();

  // Receives read completion notification from the |io_handler_|.
  void OnAsyncReadComplete(const std::string& data,
                           api::serial::ReceiveError error);

  // Receives write completion notification from the |io_handler_|.
  void OnAsyncWriteComplete(int bytes_sent, api::serial::SendError error);

  // The pathname of the serial device.
  std::string port_;

  // File handle for the opened serial device. This value is only modified from
  // the IO thread.
  base::PlatformFile file_;

  // Flag indicating whether or not the connection should persist when
  // its host app is suspended.
  bool persistent_;

  // User-specified connection name.
  std::string name_;

  // Size of the receive buffer.
  int buffer_size_;

  // Amount of time (in ms) to wait for a Read to succeed before triggering a
  // timeout response via onReceiveError.
  int receive_timeout_;

  // Amount of time (in ms) to wait for a Write to succeed before triggering
  // a timeout response.
  int send_timeout_;

  // Flag indicating that the connection is paused. A paused connection will not
  // raise new onReceive events.
  bool paused_;

  // Callback to handle the completion of a pending Open() request.
  OpenCompleteCallback open_complete_;

  // Callback to handle the completion of a pending Receive() request.
  ReceiveCompleteCallback receive_complete_;

  // Callback to handle the completion of a pending Send() request.
  SendCompleteCallback send_complete_;

  // Closure which will trigger a receive timeout unless cancelled. Reset on
  // initialization and after every successful Receive().
  scoped_ptr<TimeoutTask> receive_timeout_task_;

  // Write timeout closure. Reset on initialization and after every successful
  // Send().
  scoped_ptr<TimeoutTask> send_timeout_task_;

  // Asynchronous I/O handler.
  scoped_refptr<SerialIoHandler> io_handler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
