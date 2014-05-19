// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_PRIVATE_IDENTITY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_PRIVATE_IDENTITY_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class IdentityPrivateGetStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identityPrivate.getStrings",
                             IDENTITYPRIVATE_GETSTRINGS);
  IdentityPrivateGetStringsFunction();

 protected:
  virtual ~IdentityPrivateGetStringsFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentityPrivateGetStringsFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_PRIVATE_IDENTITY_PRIVATE_API_H_
