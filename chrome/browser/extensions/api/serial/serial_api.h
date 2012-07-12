// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/common/extensions/api/experimental_serial.h"
#include "net/base/io_buffer.h"

namespace extensions {

class APIResourceEventNotifier;
class SerialConnection;

extern const char kConnectionIdKey[];

class SerialGetPortsFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.getPorts")

  SerialGetPortsFunction();

 protected:
  virtual ~SerialGetPortsFunction() {}

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;
};

class SerialOpenFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.open")

  SerialOpenFunction();

 protected:
  virtual ~SerialOpenFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

  // Overrideable for testing.
  virtual SerialConnection* CreateSerialConnection(
      const std::string& port,
      int bitrate,
      APIResourceEventNotifier* event_notifier);
  virtual bool DoesPortExist(const std::string& port);

 private:
  scoped_ptr<api::experimental_serial::Open::Params> params_;
  int src_id_;
  int bitrate_;

  // SerialConnection will take ownership.
  APIResourceEventNotifier* event_notifier_;
};

class SerialCloseFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.close")

  SerialCloseFunction();

 protected:
  virtual ~SerialCloseFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::experimental_serial::Close::Params> params_;
};

class SerialReadFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.read")

  SerialReadFunction();

 protected:
  virtual ~SerialReadFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::experimental_serial::Read::Params> params_;
};

class SerialWriteFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.write")

  SerialWriteFunction();

 protected:
  virtual ~SerialWriteFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::experimental_serial::Write::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SerialFlushFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.flush")

  SerialFlushFunction();

 protected:
  virtual ~SerialFlushFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::experimental_serial::Flush::Params> params_;
};

class SerialGetControlSignalsFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.getControlSignals")

  SerialGetControlSignalsFunction();

 protected:
  virtual ~SerialGetControlSignalsFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::experimental_serial::GetControlSignals::Params> params_;
  bool api_response_;
};

class SerialSetControlSignalsFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.setControlSignals")

  SerialSetControlSignalsFunction();

 protected:
  virtual ~SerialSetControlSignalsFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<api::experimental_serial::SetControlSignals::Params> params_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
