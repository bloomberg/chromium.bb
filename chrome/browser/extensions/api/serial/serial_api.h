// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/api_function.h"

namespace extensions {

extern const char kConnectionIdKey[];

class SerialOpenFunction : public AsyncIOAPIFunction {
 public:
  SerialOpenFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int src_id_;
  std::string port_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.open")
};

class SerialCloseFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int connection_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.close")
};

class SerialReadFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int connection_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.read")
};

class SerialWriteFunction : public AsyncIOAPIFunction {
 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  int connection_id_;
  std::string data_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.write")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
