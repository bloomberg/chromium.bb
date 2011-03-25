// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Proxy API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_CONSTANTS_H_
#pragma once

namespace extension_proxy_api_constants {

// String literals in dictionaries used to communicate with extension.
extern const char kProxyCfgMode[];
extern const char kProxyCfgPacScript[];
extern const char kProxyCfgPacScriptUrl[];
extern const char kProxyCfgPacScriptData[];
extern const char kProxyCfgRules[];
extern const char kProxyCfgRuleHost[];
extern const char kProxyCfgRulePort[];
extern const char kProxyCfgBypassList[];
extern const char kProxyCfgScheme[];
extern const char kProxyCfgValue[];

extern const char kProxyEventFatal[];
extern const char kProxyEventError[];
extern const char kProxyEventDetails[];
extern const char kProxyEventOnProxyError[];

// Prefix that identifies PAC-script encoding urls.
extern const char kPACDataUrlPrefix[];

// The scheme for which to use a manually specified proxy, not of the proxy URI
// itself.
enum {
  SCHEME_ALL = 0,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FTP,
  SCHEME_FALLBACK,
  SCHEME_MAX = SCHEME_FALLBACK  // Keep this value up to date.
};

// The names of the JavaScript properties to extract from the proxy_rules.
// These must be kept in sync with the SCHEME_* constants.
extern const char* field_name[];

// The names of the schemes to be used to build the preference value string
// for manual proxy settings.  These must be kept in sync with the SCHEME_*
// constants.
extern const char* scheme_name[];

}  // namespace extension_proxy_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_CONSTANTS_H_
