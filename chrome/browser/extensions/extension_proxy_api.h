// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_

#include <string>

#include "base/singleton.h"
#include "chrome/browser/extensions/extension_preference_api.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "net/proxy/proxy_config.h"

class DictionaryValue;
class ExtensionEventRouterForwarder;

// Class to convert between the representation of proxy settings used
// in the Proxy Settings API and the representation used in the PrefStores.
class ProxyPrefTransformer : public PrefTransformerInterface {
 public:
  ProxyPrefTransformer();
  virtual ~ProxyPrefTransformer();

  // Implementation of PrefTransformerInterface.
  virtual Value* ExtensionToBrowserPref(const Value* extension_pref,
                                        std::string* error) OVERRIDE;
  virtual Value* BrowserToExtensionPref(const Value* browser_pref) OVERRIDE;

 private:
  // Helper functions for extension->browser pref transformation:

  // The following functions extract one piece of data from the |proxy_config|
  // each. |proxy_config| is a ProxyConfig dictionary as defined in the
  // extension API.
  //
  // - If there are NO entries for the respective pieces of data, the functions
  //   return true.
  // - If there ARE entries and they could be parsed, the functions set |out|
  //   and return true.
  // - If there are entries that could not be parsed, the functions set |error|
  //   and return false.
  bool GetProxyModeFromExtensionPref(const DictionaryValue* proxy_config,
                                     ProxyPrefs::ProxyMode* out,
                                     std::string* error) const;
  bool GetPacUrlFromExtensionPref(const DictionaryValue* proxy_config,
                                  std::string* out,
                                  std::string* error) const;
  bool GetPacDataFromExtensionPref(const DictionaryValue* proxy_config,
                                   std::string* out,
                                   std::string* error) const;
  bool GetProxyRulesStringFromExtensionPref(const DictionaryValue* proxy_config,
                                            std::string* out,
                                            std::string* error) const;
  bool GetBypassListFromExtensionPref(const DictionaryValue* proxy_config,
                                      std::string *out,
                                      std::string* error) const;

  // Creates a Browser Pref dictionary from the given parameters. Depending on
  // the value of |mode_enum|, several of the strings may be empty.
  Value* ExtensionToBrowserPref(ProxyPrefs::ProxyMode mode_enum,
                                const std::string& pac_url,
                                const std::string& pac_data,
                                const std::string& proxy_rules_string,
                                const std::string& bypass_list,
                                std::string* error) const;

  // Converts a ProxyServer dictionary instance |dict| as passed by the API
  // caller (e.g. for the http proxy in the rules element) to a
  // net::ProxyServer.
  // Returns true if successful and sets |error| otherwise.
  bool GetProxyServer(const DictionaryValue* dict,
                      net::ProxyServer::Scheme default_scheme,
                      net::ProxyServer* proxy_server,
                      std::string* error) const;

  // Joins a list of URLs (stored as StringValues) in |list| with |joiner|
  // to |out|. Returns true if successful and sets |error| otherwise.
  bool JoinUrlList(ListValue* list,
                   const std::string& joiner,
                   std::string* out,
                   std::string* error) const;


  // Helper functions for browser->extension pref transformation:

  // Converts a net::ProxyConfig::ProxyRules object to a ProxyRules dictionary
  // as defined in the extension API. Returns true if successful.
  bool ConvertProxyRules(const net::ProxyConfig::ProxyRules& config,
                         DictionaryValue* extension_proxy_rules) const;

  // Converts a net::ProxyServer object to a new ProxyServer dictionary as
  // defined in the extension API.
  void ConvertProxyServer(const net::ProxyServer& proxy,
                          DictionaryValue** extension_proxy_server) const;

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
