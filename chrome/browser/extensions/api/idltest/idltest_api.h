// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLTEST_IDLTEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDLTEST_IDLTEST_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class IdltestSendArrayBufferFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.idltest.sendArrayBuffer")

 protected:
  virtual ~IdltestSendArrayBufferFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class IdltestSendArrayBufferViewFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.idltest.sendArrayBufferView")

 protected:
  virtual ~IdltestSendArrayBufferViewFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class IdltestGetArrayBufferFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.idltest.getArrayBuffer")

 protected:
  virtual ~IdltestGetArrayBufferFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLTEST_IDLTEST_API_H_
