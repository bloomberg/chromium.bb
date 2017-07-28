// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_ACCOUNTS_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_ACCOUNTS_FUNCTION_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "services/identity/public/interfaces/identity_manager.mojom.h"

namespace extensions {

class IdentityGetAccountsFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAccounts",
                             IDENTITY_GETACCOUNTS);

  IdentityGetAccountsFunction();

 private:
  ~IdentityGetAccountsFunction() override;

  // Invoked in response to IdentityManager::GetAccounts().
  void OnGotAccounts(std::vector<identity::mojom::AccountPtr> accounts);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;

  identity::mojom::IdentityManagerPtr identity_manager_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_ACCOUNTS_FUNCTION_H_
