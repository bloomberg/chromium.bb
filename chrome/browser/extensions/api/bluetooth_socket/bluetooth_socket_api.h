// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {
namespace api {

class BluetoothSocketCreateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.create", BLUETOOTHSOCKET_CREATE);

 protected:
  virtual ~BluetoothSocketCreateFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketUpdateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.update", BLUETOOTHSOCKET_UPDATE);

 protected:
  virtual ~BluetoothSocketUpdateFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketSetPausedFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.setPaused",
                             BLUETOOTHSOCKET_SETPAUSED);

 protected:
  virtual ~BluetoothSocketSetPausedFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
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

class BluetoothSocketDisconnectFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.disconnect",
                             BLUETOOTHSOCKET_DISCONNECT);

 protected:
  virtual ~BluetoothSocketDisconnectFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketCloseFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.close", BLUETOOTHSOCKET_CLOSE);

 protected:
  virtual ~BluetoothSocketCloseFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketSendFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.send", BLUETOOTHSOCKET_SEND);

 protected:
  virtual ~BluetoothSocketSendFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketGetInfoFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getInfo",
                             BLUETOOTHSOCKET_GETINFO);

 protected:
  virtual ~BluetoothSocketGetInfoFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSocketGetSocketsFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothSocket.getSockets",
                             BLUETOOTHSOCKET_GETSOCKETS);

 protected:
  virtual ~BluetoothSocketGetSocketsFunction() {}

  // UIThreadExtensionFunction override:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_SOCKET_BLUETOOTH_SOCKET_API_H_
