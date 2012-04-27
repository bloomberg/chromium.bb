// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "net/base/io_buffer.h"

namespace extensions {

extern const char kConnectionIdKey[];

class SerialOpenFunction : public AsyncIOAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.open")

  SerialOpenFunction();

 protected:
  virtual ~SerialOpenFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int src_id_;
  std::string port_;
};

class SerialCloseFunction : public AsyncIOAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.close")

 protected:
  virtual ~SerialCloseFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int connection_id_;
};

class SerialReadFunction : public AsyncIOAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.read")

 protected:
  virtual ~SerialReadFunction() {}

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int connection_id_;
};

class SerialWriteFunction : public AsyncIOAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.write")

  SerialWriteFunction();

 protected:
  virtual ~SerialWriteFunction();

  // AsyncIOAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int connection_id_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
