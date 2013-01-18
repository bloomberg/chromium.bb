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
#include "net/base/io_buffer.h"

namespace extensions {

class SerialConnection;

extern const char kConnectionIdKey[];

class SerialAsyncApiFunction : public AsyncApiFunction {
 public:
  SerialAsyncApiFunction();

 protected:
  virtual ~SerialAsyncApiFunction();

  // AsyncApiFunction:
  virtual bool PrePrepare() OVERRIDE;

  SerialConnection* GetSerialConnection(int api_resource_id);
  void RemoveSerialConnection(int api_resource_id);

  ApiResourceManager<SerialConnection>* manager_;
};

class SerialGetPortsFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.getPorts", SERIAL_GETPORTS)

  SerialGetPortsFunction();

 protected:
  virtual ~SerialGetPortsFunction() {}

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;
};

class SerialOpenFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.open", SERIAL_OPEN)

  SerialOpenFunction();

 protected:
  virtual ~SerialOpenFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

  // Overrideable for testing.
  virtual SerialConnection* CreateSerialConnection(
      const std::string& port,
      int bitrate,
      const std::string& owner_extension_id);
  virtual bool DoesPortExist(const std::string& port);

 private:
  scoped_ptr<api::serial::Open::Params> params_;
  int bitrate_;
};

class SerialCloseFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.close", SERIAL_CLOSE)

  SerialCloseFunction();

 protected:
  virtual ~SerialCloseFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::serial::Close::Params> params_;
};

class SerialReadFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.read", SERIAL_READ)

  SerialReadFunction();

 protected:
  virtual ~SerialReadFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::serial::Read::Params> params_;
};

class SerialWriteFunction : public SerialAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("serial.write", SERIAL_WRITE)

  SerialWriteFunction();

 protected:
  virtual ~SerialWriteFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::serial::Write::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
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
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::serial::Flush::Params> params_;
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
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::serial::GetControlSignals::Params> params_;
  bool api_response_;
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
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::serial::SetControlSignals::Params> params_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
