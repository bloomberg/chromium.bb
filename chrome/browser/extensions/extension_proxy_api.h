// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_

#include <string>

#include "base/singleton.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/profiles/profile.h"
#include "net/proxy/proxy_config.h"

class DictionaryValue;
class ExtensionEventRouterForwarder;

class ProxyPrefTransformer : public PrefTransformerInterface {
 public:
  ProxyPrefTransformer();
  virtual ~ProxyPrefTransformer();

  // Implementation of PrefTransformerInterface.
  virtual Value* ExtensionToBrowserPref(const Value* extension_pref,
                                        std::string* error);
  virtual Value* BrowserToExtensionPref(const Value* browser_pref);

 private:
  // Helper functions for extension->browser pref transformation:

  // Converts a proxy "rules" element passed by the API caller into a proxy
  // configuration string that can be used by the proxy subsystem (see
  // proxy_config.h). Returns true if successful and sets |error| otherwise.
  bool GetProxyRules(DictionaryValue* proxy_rules,
                     std::string* out,
                     std::string* error);

  // Converts a proxy server description |dict| as passed by the API caller
  // (e.g. for the http proxy in the rules element) and converts it to a
  // ProxyServer. Returns true if successful and sets |error| otherwise.
  bool GetProxyServer(const DictionaryValue* dict,
                      net::ProxyServer::Scheme default_scheme,
                      net::ProxyServer* proxy_server,
                      std::string* error);

  // Joins a list of URLs (stored as StringValues) with |joiner| to |out|.
  // Returns true if successful and sets |error| otherwise.
  bool JoinUrlList(ListValue* list,
                   const std::string& joiner,
                   std::string* out,
                   std::string* error);

  // Creates a string of the "bypassList" entries of a ProxyRules object (see
  // API documentation) by joining the elements with commas.
  // Returns true if successful (i.e. string could be delivered or no
  // "bypassList" exists in the |proxy_rules|) and sets |error| otherwise.
  bool GetBypassList(DictionaryValue* proxy_rules,
                     std::string* out,
                     std::string* error);

  // Helper functions for browser->extension pref transformation:

  // Convert the representation of a proxy configuration from the format
  // that is stored in the pref stores to the format that is used by the API.
  // See ProxyServer type defined in |experimental.proxy|.
  bool ConvertToApiFormat(const DictionaryValue* proxy_prefs,
                          DictionaryValue* api_proxy_config);
  bool ParseRules(const std::string& rules, DictionaryValue* out) const;
  DictionaryValue* ConvertToDictionary(const net::ProxyServer& proxy) const;

  DISALLOW_COPY_AND_ASSIGN(ProxyPrefTransformer);
};

// This class observes proxy error events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionProxyEventRouter {
 public:
  static ExtensionProxyEventRouter* GetInstance();

  void OnProxyError(ExtensionEventRouterForwarder* event_router,
                    ProfileId profile_id,
                    int error_code);

 private:
  friend struct DefaultSingletonTraits<ExtensionProxyEventRouter>;

  ExtensionProxyEventRouter();
  ~ExtensionProxyEventRouter();

  DISALLOW_COPY_AND_ASSIGN(ExtensionProxyEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
