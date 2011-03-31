// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of helper functions for the Chrome Extensions Proxy Settings API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_HELPERS_H_
#pragma once

#include <string>

#include "chrome/browser/prefs/proxy_prefs.h"
#include "net/proxy/proxy_config.h"

class DictionaryValue;
class ListValue;
class ProxyConfigDictionary;

namespace extension_proxy_api_helpers {

// Conversion between PAC scripts and data-encoding URLs containing these
// PAC scripts. Data-encoding URLs consist of a data:// prefix, a mime-type and
// base64 encoded text. The functions return true in case of success.
// CreatePACScriptFromDataURL should only be called on data-encoding urls
// created with CreateDataURLFromPACScript.
bool CreateDataURLFromPACScript(const std::string& pac_script,
                                std::string* pac_script_url_base64_encoded);
bool CreatePACScriptFromDataURL(
    const std::string& pac_script_url_base64_encoded,
    std::string* pac_script);

// Helper functions for extension->browser pref transformation:

// The following functions extract one piece of data from the |proxy_config|
// each. |proxy_config| is a ProxyConfig dictionary as defined in the
// extension API. All output values conform to the format expected by a
// ProxyConfigDictionary.
//
// - If there are NO entries for the respective pieces of data, the functions
//   return true.
// - If there ARE entries and they could be parsed, the functions set |out|
//   and return true.
// - If there are entries that could not be parsed, the functions set |error|
//   and return false.
bool GetProxyModeFromExtensionPref(const DictionaryValue* proxy_config,
                                   ProxyPrefs::ProxyMode* out,
                                   std::string* error);
bool GetPacUrlFromExtensionPref(const DictionaryValue* proxy_config,
                                std::string* out,
                                std::string* error);
bool GetPacDataFromExtensionPref(const DictionaryValue* proxy_config,
                                 std::string* out,
                                 std::string* error);
bool GetProxyRulesStringFromExtensionPref(const DictionaryValue* proxy_config,
                                          std::string* out,
                                          std::string* error);
bool GetBypassListFromExtensionPref(const DictionaryValue* proxy_config,
                                    std::string* out,
                                    std::string* error);

// Creates and returns a ProxyConfig dictionary (as defined in the extension
// API) from the given parameters. Ownership is passed to the caller.
// Depending on the value of |mode_enum|, several of the strings may be empty.
DictionaryValue* CreateProxyConfigDict(ProxyPrefs::ProxyMode mode_enum,
                                       const std::string& pac_url,
                                       const std::string& pac_data,
                                       const std::string& proxy_rules_string,
                                       const std::string& bypass_list,
                                       std::string* error);

// Converts a ProxyServer dictionary instance (as defined in the extension API)
// |proxy_server| to a net::ProxyServer.
// |default_scheme| is the default scheme that is filled in, in case the
// caller did not pass one.
// Returns true if successful and sets |error| otherwise.
bool GetProxyServer(const DictionaryValue* proxy_server,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* out,
                    std::string* error);

// Joins a list of URLs (stored as StringValues) in |list| with |joiner|
// to |out|. Returns true if successful and sets |error| otherwise.
bool JoinUrlList(ListValue* list,
                 const std::string& joiner,
                 std::string* out,
                 std::string* error);


// Helper functions for browser->extension pref transformation:

// Creates and returns a ProxyRules dictionary as defined in the extension API
// with the values of a ProxyConfigDictionary configured for fixed proxy
// servers. Returns NULL in case of failures. Ownership is passed to the caller.
DictionaryValue* CreateProxyRulesDict(
    const ProxyConfigDictionary& proxy_config);

// Creates and returns a ProxyServer dictionary as defined in the extension API
// with values from a net::ProxyServer object. Never returns NULL. Ownership is
// passed to the caller.
DictionaryValue* CreateProxyServerDict(const net::ProxyServer& proxy);

// Creates and returns a PacScript dictionary as defined in the extension API
// with the values of a ProxyconfigDictionary configured for pac scripts.
// Returns NULL in case of failures. Ownership is passed to the caller.
DictionaryValue* CreatePacScriptDict(const ProxyConfigDictionary& proxy_config);

// Tokenizes the |in| at delimiters |delims| and returns a new ListValue with
// StringValues created from the tokens. Ownership is passed to the caller.
ListValue* TokenizeToStringList(const std::string& in,
                                const std::string& delims);

}  // namespace extension_proxy_api_helpers

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_HELPERS_H_
