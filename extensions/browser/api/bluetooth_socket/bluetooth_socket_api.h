// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/common/api/bluetooth_socket.h"

namespace device {
class BluetoothSocket;
}

namespace net {
class IOBuffer;
}

namespace extensions {

namespace core_api {

class BluetoothSocketEventDispatcher;

// Asynchronous API function that performs its work on the BluetoothApiSocket
// thread while providing methods to manage resources of that class. This
// follows the pattern of AsyncApiFunction, but does not derive from it,
// because BluetoothApiSocket methods must be called on the UI Thread.
class BluetoothSocketAsyncApiFunction : public AsyncExtensionFunction {
 public:
  BluetoothSocketAsyncApiFunction();

 protected:
  virtual ~BluetoothSocketAsyncApiFunction();

  // AsyncExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  bool PrePrepare();
  bool Respond();
  void AsyncWorkCompleted();

  virtual bool Prepare() = 0;
  virtual void Work();
  virtual void AsyncWorkStart();

  content::BrowserThread::ID work_thread_id() const;

  int AddSocket(BluetoothApiSocket* socket);
  BluetoothApiSocket* GetSocket(int api_resource_id);
  void RemoveSocket(int api_resource_id);
  base::hash_set<int>* GetSocketIds();

 private:
  ApiResourceManager<BluetoothApiSocket>* manager_;
};

class BluetoothSocketCreateFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.create", BLUETOOTHSOCKET_CREATE);

  BluetoothSocketCreateFunction();

 protected:
  virtual ~BluetoothSocketCreateFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<bluetooth_socket::Create::Params> params_;
};

class BluetoothSocketUpdateFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.update", BLUETOOTHSOCKET_UPDATE);

  BluetoothSocketUpdateFunction();

 protected:
  virtual ~BluetoothSocketUpdateFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<bluetooth_socket::Update::Params> params_;
};

class BluetoothSocketSetPausedFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.setPaused",
                             BLUETOOTHSOCKET_SETPAUSED);

  BluetoothSocketSetPausedFunction();

 protected:
  virtual ~BluetoothSocketSetPausedFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<bluetooth_socket::SetPaused::Params> params_;
  BluetoothSocketEventDispatcher* socket_event_dispatcher_;
};

class BluetoothSocketListenFunction : public BluetoothSocketAsyncApiFunction {
 public:
  BluetoothSocketListenFunction();

  virtual bool CreateParams() = 0;
  virtual void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      scoped_ptr<std::string> name,
      const device::BluetoothAdapter::CreateServiceCallback& callback,
      const device::BluetoothAdapter::CreateServiceErrorCallback&
          error_callback) = 0;
  virtual void CreateResults() = 0;

  virtual int socket_id() const = 0;
  virtual const std::string& uuid() const = 0;

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 protected:
  virtual ~BluetoothSocketListenFunction();

  virtual void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  virtual void OnCreateService(scoped_refptr<device::BluetoothSocket> socket);
  virtual void OnCreateServiceError(const std::string& message);

  BluetoothSocketEventDispatcher* socket_event_dispatcher_;
};

class BluetoothSocketListenUsingRfcommFunction
    : public BluetoothSocketListenFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingRfcomm",
                             BLUETOOTHSOCKET_LISTENUSINGRFCOMM);

  BluetoothSocketListenUsingRfcommFunction();

  // BluetoothSocketListenFunction:
  virtual int socket_id() const OVERRIDE;
  virtual const std::string& uuid() const OVERRIDE;

  virtual bool CreateParams() OVERRIDE;
  virtual void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      scoped_ptr<std::string> name,
      const device::BluetoothAdapter::CreateServiceCallback& callback,
      const device::BluetoothAdapter::CreateServiceErrorCallback&
          error_callback) OVERRIDE;
  virtual void CreateResults() OVERRIDE;

 protected:
  virtual ~BluetoothSocketListenUsingRfcommFunction();

 private:
  scoped_ptr<bluetooth_socket::ListenUsingRfcomm::Params> params_;
};

class BluetoothSocketListenUsingL2capFunction
    : public BluetoothSocketListenFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingL2cap",
                             BLUETOOTHSOCKET_LISTENUSINGL2CAP);

  BluetoothSocketListenUsingL2capFunction();

  // BluetoothSocketListenFunction:
  virtual int socket_id() const OVERRIDE;
  virtual const std::string& uuid() const OVERRIDE;

  virtual bool CreateParams() OVERRIDE;
  virtual void CreateService(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID& uuid,
      scoped_ptr<std::string> name,
      const device::BluetoothAdapter::CreateServiceCallback& callback,
      const device::BluetoothAdapter::CreateServiceErrorCallback&
          error_callback) OVERRIDE;
  virtual void CreateResults() OVERRIDE;

 protected:
  virtual ~BluetoothSocketListenUsingL2capFunction();

 private:
  scoped_ptr<bluetooth_socket::ListenUsingL2cap::Params> params_;
};

class BluetoothSocketAbstractConnectFunction :
    public BluetoothSocketAsyncApiFunction {
 public:
  BluetoothSocketAbstractConnectFunction();

 protected:
  virtual ~BluetoothSocketAbstractConnectFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  // Subclasses should implement this method to connect to the service
  // registered with |uuid| on the |device|.
  virtual void ConnectToService(device::BluetoothDevice* device,
                                const device::BluetoothUUID& uuid) = 0;

  virtual void OnConnect(scoped_refptr<device::BluetoothSocket> socket);
  virtual void OnConnectError(const std::string& message);

 private:
  virtual void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  scoped_ptr<bluetooth_socket::Connect::Params> params_;
  BluetoothSocketEventDispatcher* socket_event_dispatcher_;
};

class BluetoothSocketConnectFunction :
    public BluetoothSocketAbstractConnectFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.connect",
                             BLUETOOTHSOCKET_CONNECT);

  BluetoothSocketConnectFunction();

 protected:
  virtual ~BluetoothSocketConnectFunction();

  // BluetoothSocketAbstractConnectFunction:
  virtual void ConnectToService(device::BluetoothDevice* device,
                                const device::BluetoothUUID& uuid) OVERRIDE;
};

class BluetoothSocketDisconnectFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.disconnect",
                             BLUETOOTHSOCKET_DISCONNECT);

  BluetoothSocketDisconnectFunction();

 protected:
  virtual ~BluetoothSocketDisconnectFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual void OnSuccess();

  scoped_ptr<bluetooth_socket::Disconnect::Params> params_;
};

class BluetoothSocketCloseFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.close", BLUETOOTHSOCKET_CLOSE);

  BluetoothSocketCloseFunction();

 protected:
  virtual ~BluetoothSocketCloseFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<bluetooth_socket::Close::Params> params_;
};

class BluetoothSocketSendFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.send", BLUETOOTHSOCKET_SEND);

  BluetoothSocketSendFunction();

 protected:
  virtual ~BluetoothSocketSendFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  void OnSuccess(int bytes_sent);
  void OnError(BluetoothApiSocket::ErrorReason reason,
               const std::string& message);

  scoped_ptr<bluetooth_socket::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class BluetoothSocketGetInfoFunction : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getInfo",
                             BLUETOOTHSOCKET_GETINFO);

  BluetoothSocketGetInfoFunction();

 protected:
  virtual ~BluetoothSocketGetInfoFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<bluetooth_socket::GetInfo::Params> params_;
};

class BluetoothSocketGetSocketsFunction
    : public BluetoothSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getSockets",
                             BLUETOOTHSOCKET_GETSOCKETS);

  BluetoothSocketGetSocketsFunction();

 protected:
  virtual ~BluetoothSocketGetSocketsFunction();

  // BluetoothSocketAsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
};

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
