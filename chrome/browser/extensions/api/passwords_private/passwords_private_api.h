// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class PasswordsPrivateCanPasswordAccountBeManagedFunction :
    public UIThreadExtensionFunction {
 public:
  PasswordsPrivateCanPasswordAccountBeManagedFunction() {}
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.canPasswordAccountBeManaged",
                             PASSWORDSPRIVATE_CANPASSWORDACCOUNTBEMANAGED);

 protected:
  ~PasswordsPrivateCanPasswordAccountBeManagedFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateCanPasswordAccountBeManagedFunction);
};

class PasswordsPrivateRemoveSavedPasswordFunction :
    public UIThreadExtensionFunction {
 public:
  PasswordsPrivateRemoveSavedPasswordFunction() {}
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removeSavedPassword",
                             PASSWORDSPRIVATE_REMOVESAVEDPASSWORD);

 protected:
  ~PasswordsPrivateRemoveSavedPasswordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateRemoveSavedPasswordFunction);
};

class PasswordsPrivateRemovePasswordExceptionFunction :
    public UIThreadExtensionFunction {
 public:
  PasswordsPrivateRemovePasswordExceptionFunction() {}
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.removePasswordException",
                             PASSWORDSPRIVATE_REMOVEPASSWORDEXCEPTION);

 protected:
  ~PasswordsPrivateRemovePasswordExceptionFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateRemovePasswordExceptionFunction);
};

class PasswordsPrivateGetPlaintextPasswordFunction :
    public UIThreadExtensionFunction {
 public:
  PasswordsPrivateGetPlaintextPasswordFunction() {}
  DECLARE_EXTENSION_FUNCTION("passwordsPrivate.GetPlaintextPassword",
                             PASSWORDSPRIVATE_GETPLAINTEXTPASSWORD);

 protected:
  ~PasswordsPrivateGetPlaintextPasswordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateGetPlaintextPasswordFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_API_H_
