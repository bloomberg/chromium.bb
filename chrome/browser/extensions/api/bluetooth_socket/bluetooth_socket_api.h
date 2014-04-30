// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"
#include "chrome/common/extensions/api/bluetooth_socket.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace net {
class IOBuffer;
}

namespace extensions {

namespace api {

class BluetoothSocketEventDispatcher;

// Asynchronous API function that performs its work on the BluetoothApiSocket
// thread while providing methods to manage resources of that class. This
// follows the pattern of AsyncApiFunction, but does not derive from it,
// because BluetoothApiSocket methods must be called on the UI Thread.
class BluetoothSocketAsyncApiFunction : public UIThreadExtensionFunction {
 public:
  BluetoothSocketAsyncApiFunction();

 protected:
  virtual ~BluetoothSocketAsyncApiFunction();

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

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

class BluetoothSocketListenUsingRfcommFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingRfcomm",
                             BLUETOOTHSOCKET_LISTENUSINGRFCOMM);

 protected:
  virtual ~BluetoothSocketListenUsingRfcommFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketListenUsingInsecureRfcommFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingInsecureRfcomm",
                             BLUETOOTHSOCKET_LISTENUSINGINSECURERFCOMM);

 protected:
  virtual ~BluetoothSocketListenUsingInsecureRfcommFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketListenUsingL2capFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.listenUsingL2cap",
                             BLUETOOTHSOCKET_LISTENUSINGL2CAP);

 protected:
  virtual ~BluetoothSocketListenUsingL2capFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketConnectFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.connect",
                             BLUETOOTHSOCKET_CONNECT);

 protected:
  virtual ~BluetoothSocketConnectFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
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

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
