// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCM_GCM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCM_GCM_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class GcmRegisterFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcm.register", GCM_REGISTER);

  GcmRegisterFunction() {}

 protected:
  virtual ~GcmRegisterFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE FINAL;
};

class GcmSendFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcm.send", GCM_SEND);

  GcmSendFunction() {}

 protected:
  virtual ~GcmSendFunction() {}

  // Send function implementation.
  virtual bool RunImpl() OVERRIDE FINAL;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCM_GCM_API_H_
