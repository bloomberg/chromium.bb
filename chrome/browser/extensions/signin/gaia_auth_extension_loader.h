// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Manages and registers the gaia auth extension with the extension system.
class GaiaAuthExtensionLoader : public BrowserContextKeyedAPI {
 public:
  explicit GaiaAuthExtensionLoader(content::BrowserContext* context);
  virtual ~GaiaAuthExtensionLoader();

  // Load the gaia auth extension if the extension is not loaded yet.
  void LoadIfNeeded();
  // Unload the gaia auth extension if no pending reference.
  void UnloadIfNeeded();

  static GaiaAuthExtensionLoader* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader>;

  // KeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "GaiaAuthExtensionLoader";
  }
  static const bool kServiceRedirectedInIncognito = true;

  content::BrowserContext* browser_context_;
  int load_count_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthExtensionLoader);
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_
