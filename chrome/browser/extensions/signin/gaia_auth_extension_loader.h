// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

const char kGaiaAuthExtensionId[] = "mfffpogegjflfpflabcdkioaeobkgjik";
const char kGaiaAuthExtensionOrigin[] =
      "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik";

// Manages and registers the gaia auth extension with the extension system.
class GaiaAuthExtensionLoader : public BrowserContextKeyedAPI {
 public:
  explicit GaiaAuthExtensionLoader(content::BrowserContext* context);
  ~GaiaAuthExtensionLoader() override;

  // Load the gaia auth extension if the extension is not loaded yet.
  void LoadIfNeeded();
  // Unload the gaia auth extension if no pending reference.
  void UnloadIfNeeded();
  void UnloadIfNeededAsync();

  // Add a string data for gaia auth extension. Returns an ID that
  // could be used to get the data. All strings are cleared when gaia auth
  // is unloaded.
  int AddData(const std::string& data);

  // Get data for the given ID. Returns true if the data is found and
  // its value is copied to |data|. Otherwise, returns false.
  bool GetData(int data_id, std::string* data);

  static GaiaAuthExtensionLoader* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<GaiaAuthExtensionLoader>;

  // KeyedService overrides:
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "GaiaAuthExtensionLoader";
  }
  static const bool kServiceRedirectedInIncognito = true;

  content::BrowserContext* browser_context_;
  int load_count_;

  int last_data_id_;
  std::map<int, std::string> data_;

  base::WeakPtrFactory<GaiaAuthExtensionLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthExtensionLoader);
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SIGNIN_GAIA_AUTH_EXTENSION_LOADER_H_
