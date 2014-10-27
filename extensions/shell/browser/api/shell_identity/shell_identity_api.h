// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_SHELL_IDENTITY_SHELL_IDENTITY_API_H_
#define EXTENSIONS_SHELL_BROWSER_API_SHELL_IDENTITY_SHELL_IDENTITY_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// TODO(jamescook): Implement this function.
class ShellIdentityGetAuthTokenFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("shell.identity.getAuthToken", UNKNOWN);

  ShellIdentityGetAuthTokenFunction();

 protected:
  ~ShellIdentityGetAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellIdentityGetAuthTokenFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_SHELL_IDENTITY_SHELL_IDENTITY_API_H_
