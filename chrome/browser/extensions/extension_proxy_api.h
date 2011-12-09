// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Proxy Settings API relevant classes to realize
// the API as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_

#include <string>

#include "base/memory/singleton.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/prefs/proxy_prefs.h"

class ExtensionEventRouterForwarder;

namespace base {
class Value;
}

// Class to convert between the representation of proxy settings used
// in the Proxy Settings API and the representation used in the PrefStores.
// This plugs into the ExtensionPreferenceAPI to get and set proxy settings.
class ProxyPrefTransformer : public PrefTransformerInterface {
 public:
  ProxyPrefTransformer();
  virtual ~ProxyPrefTransformer();

  // Implementation of PrefTransformerInterface.
  virtual base::Value* ExtensionToBrowserPref(const base::Value* extension_pref,
                                              std::string* error,
                                              bool* bad_message) OVERRIDE;
  virtual base::Value* BrowserToExtensionPref(
      const base::Value* browser_pref) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyPrefTransformer);
};

// This class observes proxy error events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionProxyEventRouter {
 public:
  static ExtensionProxyEventRouter* GetInstance();

  void OnProxyError(ExtensionEventRouterForwarder* event_router,
                    void* profile,
                    int error_code);

  void OnPACScriptError(ExtensionEventRouterForwarder* event_router,
                        void* profile,
                        int line_number,
                        const string16& error);

 private:
  friend struct DefaultSingletonTraits<ExtensionProxyEventRouter>;

  ExtensionProxyEventRouter();
  ~ExtensionProxyEventRouter();

  DISALLOW_COPY_AND_ASSIGN(ExtensionProxyEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
