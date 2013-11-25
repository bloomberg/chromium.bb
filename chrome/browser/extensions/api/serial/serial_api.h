// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/common/extensions/api/serial.h"

namespace extensions {

class SerialConnection;

namespace api {

class SerialEventDispatcher;

class SerialAsyncApiFunction : public AsyncApiFunction {
 public:
  SerialAsyncApiFunction();

 protected:
  virtual ~SerialAsyncApiFunction();

  // AsyncApiFunction:
  virtual bool PrePrepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;

  SerialConnection* GetSerialConnection(int api_resource_id);
  void RemoveSerialConnection(int api_resource_id);

  ApiResourceManager<SerialConnection>* manager_;
};

class SerialGetDevicesFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getDevices", SERIAL_GETDEVICES)

  SerialGetDevicesFunction();

 protected:
  virtual ~SerialGetDevicesFunction() {}

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
};

class SerialConnectFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.connect", SERIAL_CONNECT)

  SerialConnectFunction();

 protected:
  virtual ~SerialConnectFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  virtual SerialConnection* CreateSerialConnection(
      const std::string& port,
      const std::string& extension_id) const;

 private:
  void OnConnected(bool success);
  void FinishConnect();

  scoped_ptr<serial::Connect::Params> params_;

  // SerialEventDispatcher is owned by a Profile.
  SerialEventDispatcher* serial_event_dispatcher_;

  // This connection is created within SerialConnectFunction.
  // From there it is either destroyed in OnConnected (upon failure)
  // or its ownership is transferred to the profile's.
  // ApiResourceManager<SerialConnection>.
  SerialConnection* connection_;
};

class SerialUpdateFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.update", SERIAL_UPDATE);

  SerialUpdateFunction();

 protected:
  virtual ~SerialUpdateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::Update::Params> params_;
};

class SerialDisconnectFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.disconnect", SERIAL_DISCONNECT)

  SerialDisconnectFunction();

 protected:
  virtual ~SerialDisconnectFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::Disconnect::Params> params_;
};

class SerialSetPausedFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.setPaused", SERIAL_SETPAUSED)

  SerialSetPausedFunction();

 protected:
  virtual ~SerialSetPausedFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::SetPaused::Params> params_;
  SerialEventDispatcher* serial_event_dispatcher_;
};

class SerialGetInfoFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getInfo", SERIAL_GETINFO)

  SerialGetInfoFunction();

 protected:
  virtual ~SerialGetInfoFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::GetInfo::Params> params_;
};

class SerialGetConnectionsFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getConnections", SERIAL_GETCONNECTIONS);

  SerialGetConnectionsFunction();

 protected:
  virtual ~SerialGetConnectionsFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
};

class SerialSendFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.send", SERIAL_SEND)

  SerialSendFunction();

 protected:
  virtual ~SerialSendFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  void OnSendComplete(int bytes_sent, serial::SendError error);

  scoped_ptr<serial::Send::Params> params_;
};

class SerialFlushFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.flush", SERIAL_FLUSH)

  SerialFlushFunction();

 protected:
  virtual ~SerialFlushFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::Flush::Params> params_;
};

class SerialGetControlSignalsFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getControlSignals",
                             SERIAL_GETCONTROLSIGNALS)

  SerialGetControlSignalsFunction();

 protected:
  virtual ~SerialGetControlSignalsFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::GetControlSignals::Params> params_;
};

class SerialSetControlSignalsFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.setControlSignals",
                             SERIAL_SETCONTROLSIGNALS)

  SerialSetControlSignalsFunction();

 protected:
  virtual ~SerialSetControlSignalsFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<serial::SetControlSignals::Params> params_;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
