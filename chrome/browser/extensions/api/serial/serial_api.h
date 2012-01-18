// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

extern const char kConnectionIdKey[];

class SerialOpenFunction : public AsyncExtensionFunction {
 public:
  SerialOpenFunction();
  virtual ~SerialOpenFunction();

  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

 private:
  std::string port_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.open")
};

class SerialCloseFunction : public AsyncExtensionFunction {
 public:
  SerialCloseFunction();
  virtual ~SerialCloseFunction();

  virtual bool RunImpl() OVERRIDE;

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

 private:
  int connection_id_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.serial.close")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_API_H_
