// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_SOCKET_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_SOCKET_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {
struct Event;
class BluetoothApiSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "bluetooth" sockets from callback on native socket
// instances. There is one instance per browser context.
class BluetoothSocketEventDispatcher
    : public base::RefCountedThreadSafe<BluetoothSocketEventDispatcher> {
 public:
  typedef ApiResourceManager<BluetoothApiSocket>::ApiResourceData SocketData;
  explicit BluetoothSocketEventDispatcher(
      content::BrowserContext* context,
      scoped_refptr<SocketData> socket_data);

  // Socket is active again, start receiving data from it.
  void OnSocketResume(const std::string& extension_id, int socket_id);

 private:
  friend class base::RefCountedThreadSafe<BluetoothSocketEventDispatcher>;
  virtual ~BluetoothSocketEventDispatcher();

  // base::Bind supports methods with up to 6 parameters. ReceiveParams is used
  // as a workaround that limitation for invoking StartReceive.
  struct ReceiveParams {
    ReceiveParams();
    ~ReceiveParams();

    std::string extension_id;
    int socket_id;
  };

  // Start a receive and register a callback.
  void StartReceive(const ReceiveParams& params);

  // Called when socket receive data.
  void ReceiveCallback(const ReceiveParams& params,
                       int bytes_read,
                       scoped_refptr<net::IOBuffer> io_buffer);

  // Called when socket receive data.
  void ReceiveErrorCallback(const ReceiveParams& params,
                            BluetoothApiSocket::ErrorReason error_reason,
                            const std::string& error);

  // Post an extension event from IO to UI thread
  void PostEvent(const ReceiveParams& params, scoped_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  void DispatchEvent(const std::string& extension_id, scoped_ptr<Event> event);

  // Usually FILE thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  void* browser_context_id_;
  scoped_refptr<SocketData> socket_data_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketEventDispatcher);
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_SOCKET_EVENT_DISPATCHER_H_
