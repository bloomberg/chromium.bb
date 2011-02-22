// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_

#include <string>

#include "base/singleton.h"
#include "chrome/browser/extensions/extension_function.h"
#include "net/proxy/proxy_config.h"

class DictionaryValue;
class ExtensionIOEventRouter;

// This class observes proxy error events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionProxyEventRouter {
 public:
  static ExtensionProxyEventRouter* GetInstance();

  void OnProxyError(const ExtensionIOEventRouter* event_router,
                    int error_code);

 private:
  friend struct DefaultSingletonTraits<ExtensionProxyEventRouter>;

  ExtensionProxyEventRouter();
  ~ExtensionProxyEventRouter();

  DISALLOW_COPY_AND_ASSIGN(ExtensionProxyEventRouter);
};

class ProxySettingsFunction : public SyncExtensionFunction {
 public:
  virtual ~ProxySettingsFunction() {}
  virtual bool RunImpl() = 0;
 protected:
  // Takes ownership of |pref_value|.
  void ApplyPreference(
      const char* pref_path, Value* pref_value, bool incognito);
  void RemovePreference(const char* pref_path, bool incognito);
};

class UseCustomProxySettingsFunction : public ProxySettingsFunction {
 public:
  virtual ~UseCustomProxySettingsFunction() {}
  virtual bool RunImpl();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.proxy.useCustomProxySettings")
 private:
  // Converts a proxy "rules" element passed by the API caller into a proxy
  // configuration string that can be used by the proxy subsystem (see
  // proxy_config.h). Returns true if successful and sets |error_| otherwise.
  bool GetProxyRules(DictionaryValue* proxy_rules, std::string* out);

  // Converts a proxy server description |dict| as passed by the API caller
  // (e.g. for the http proxy in the rules element) and converts it to a
  // ProxyServer. Returns true if successful and sets |error_| otherwise.
  bool GetProxyServer(const DictionaryValue* dict,
                      net::ProxyServer::Scheme default_scheme,
                      net::ProxyServer* proxy_server);

  // Joins a list of URLs (stored as StringValues) with |joiner| to |out|.
  // Returns true if successful and sets |error_| otherwise.
  bool JoinUrlList(ListValue* list,
                   const std::string& joiner,
                   std::string* out);

  // Creates a string of the "bypassList" entries of a ProxyRules object (see
  // API documentation) by joining the elements with commas.
  // Returns true if successful (i.e. string could be delivered or no
  // "bypassList" exists in the |proxy_rules|) and sets |error_| otherwise.
  bool GetBypassList(DictionaryValue* proxy_rules, std::string* out);
};

class RemoveCustomProxySettingsFunction : public ProxySettingsFunction {
 public:
  virtual ~RemoveCustomProxySettingsFunction() {}
  virtual bool RunImpl();

  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.proxy.removeCustomProxySettings")
};

class GetCurrentProxySettingsFunction : public ProxySettingsFunction {
 public:
  virtual ~GetCurrentProxySettingsFunction() {}
  virtual bool RunImpl();

  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.proxy.getCurrentProxySettings")
 private:
  // Convert the representation of a proxy configuration from the format
  // that is stored in the pref stores to the format that is used by the API.
  // See ProxyServer type defined in |experimental.proxy|.
  bool ConvertToApiFormat(const DictionaryValue* proxy_prefs,
                          DictionaryValue* api_proxy_config);
  bool ParseRules(const std::string& rules, DictionaryValue* out) const;
  DictionaryValue* ConvertToDictionary(const net::ProxyServer& proxy) const;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
