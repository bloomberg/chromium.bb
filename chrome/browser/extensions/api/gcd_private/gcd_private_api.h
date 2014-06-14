// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class GcdPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit GcdPrivateAPI(content::BrowserContext* context);
  virtual ~GcdPrivateAPI();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<GcdPrivateAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<GcdPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "GcdPrivateAPI"; }

  content::BrowserContext* const browser_context_;
};

class GcdPrivateGetCloudDeviceListFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCloudDeviceList",
                             FEEDBACKPRIVATE_GETSTRINGS)

 protected:
  virtual ~GcdPrivateGetCloudDeviceListFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
