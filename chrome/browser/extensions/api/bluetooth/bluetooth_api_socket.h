// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_SOCKET_H_

#include <string>

#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

// Representation of socket instances from the "bluetooth" namespace,
// abstracting away platform differences from the underlying BluetoothSocketXxx
// class. All methods must be called on the |kThreadId| thread.
class BluetoothApiSocket : public ApiResource {
 public:
  enum ErrorReason { kSystemError, kIOPending, kDisconnected };

  typedef base::Callback<void(int)> SendCompletionCallback;
  typedef base::Callback<void(int, scoped_refptr<net::IOBuffer> io_buffer)>
      ReceiveCompletionCallback;
  typedef base::Callback<void(const std::string& error_message)>
      ErrorCompletionCallback;
  typedef base::Callback<void(ErrorReason, const std::string& error_message)>
      ReceiveErrorCompletionCallback;

  BluetoothApiSocket(const std::string& owner_extension_id,
                     scoped_refptr<device::BluetoothSocket> socket,
                     const std::string& device_address,
                     const device::BluetoothUUID& uuid);
  virtual ~BluetoothApiSocket();

  // Closes the underlying connection. This is a best effort, and never fails.
  virtual void Disconnect(const base::Closure& success_callback);

  // Receives data from the socket and calls |success_callback| when data is
  // available. |count| is maximum amount of bytes received. If an error occurs,
  // calls |error_callback| with a reason and a message. In particular, if a
  // |Receive| operation is still pending, |error_callback| will be called with
  // |kIOPending| error.
  virtual void Receive(int count,
                       const ReceiveCompletionCallback& success_callback,
                       const ReceiveErrorCompletionCallback& error_callback);

  // Sends |buffer| to the socket and calls |success_callback| when data has
  // been successfully sent. |buffer_size| is the numberof bytes contained in
  // |buffer|. If an error occurs, calls |error_callback| with a reason and a
  // message. Calling |Send| multiple times without waiting for the callbacks to
  // be called is a valid usage, as |buffer| instances are buffered until the
  // underlying communication channel is available for sending data.
  virtual void Send(scoped_refptr<net::IOBuffer> buffer,
                    int buffer_size,
                    const SendCompletionCallback& success_callback,
                    const ErrorCompletionCallback& error_callback);

  const std::string& device_address() const { return device_address_; }
  const device::BluetoothUUID& uuid() const { return uuid_; }

  // Overriden from extensions::ApiResource.
  virtual bool IsPersistent() const OVERRIDE;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  bool persistent() const { return persistent_; }
  void set_persistent(bool persistent) { persistent_ = persistent; }

  int buffer_size() const { return buffer_size_; }
  void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }

  bool paused() const { return paused_; }
  void set_paused(bool paused) { paused_ = paused; }

  // Platform specific implementations of |BluetoothSocket| require being called
  // on the UI thread.
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<BluetoothApiSocket>;
  static const char* service_name() { return "BluetoothApiSocketManager"; }

  static void OnSocketReceiveError(
      const ReceiveErrorCompletionCallback& error_callback,
      device::BluetoothSocket::ErrorReason reason,
      const std::string& message);

  // The underlying device socket instance.
  scoped_refptr<device::BluetoothSocket> socket_;

  // The address of the device this socket is connected to.
  std::string device_address_;

  // The uuid of the service this socket is connected to.
  device::BluetoothUUID uuid_;

  // Application-defined string - see bluetooth.idl.
  std::string name_;

  // Flag indicating whether the socket is left open when the application is
  // suspended - see bluetooth.idl.
  bool persistent_;

  // The size of the buffer used to receive data - see bluetooth.idl.
  int buffer_size_;

  // Flag indicating whether a connected socket blocks its peer from sending
  // more data - see bluetooth.idl.
  bool paused_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothApiSocket);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_SOCKET_H_
