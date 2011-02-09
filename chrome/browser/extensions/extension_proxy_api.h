// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

class DictionaryValue;

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
  // Temporary data container to pass structured elements between functions.
  struct ProxyServer {
    enum {
      INVALID_PORT = -1
    };
    ProxyServer() : scheme("http"), host(""), port(INVALID_PORT) {}

    // The scheme of the proxy URI itself.
    std::string scheme;
    std::string host;
    int port;
  };

  // Converts a proxy server description |dict| as passed by the API caller
  // (e.g. for the http proxy in the rules element) and converts it to a
  // ProxyServer. Returns true if successful.
  bool GetProxyServer(const DictionaryValue* dict, ProxyServer* proxy_server);

  // Converts a proxy "rules" element passed by the API caller into a proxy
  // configuration string that can be used by the proxy subsystem (see
  // proxy_config.h). Returns true if successful.
  bool GetProxyRules(DictionaryValue* proxy_rules, std::string* out);
};

class RemoveCustomProxySettingsFunction : public ProxySettingsFunction {
 public:
  virtual ~RemoveCustomProxySettingsFunction() {}
  virtual bool RunImpl();

  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.proxy.removeCustomProxySettings")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
