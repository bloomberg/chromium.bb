// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the Chrome Extensions Proxy Settings API.

#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"

#include "base/basictypes.h"

namespace extensions {
namespace proxy_api_constants {

const char kProxyConfigMode[] = "mode";
const char kProxyConfigPacScript[] = "pacScript";
const char kProxyConfigPacScriptMandatory[] = "mandatory";
const char kProxyConfigPacScriptUrl[] = "url";
const char kProxyConfigPacScriptData[] = "data";
const char kProxyConfigRules[] = "rules";
const char kProxyConfigRuleHost[] = "host";
const char kProxyConfigRulePort[] = "port";
const char kProxyConfigRuleScheme[] = "scheme";
const char kProxyConfigBypassList[] = "bypassList";

const char kProxyEventFatal[] = "fatal";
const char kProxyEventError[] = "error";
const char kProxyEventDetails[] = "details";
const char kProxyEventOnProxyError[] = "proxy.onProxyError";

const char kPACDataUrlPrefix[] =
    "data:application/x-ns-proxy-autoconfig;base64,";

const char* field_name[] = { "singleProxy",
                             "proxyForHttp",
                             "proxyForHttps",
                             "proxyForFtp",
                             "fallbackProxy" };

const char* scheme_name[] = { "*error*",
                              "http",
                              "https",
                              "ftp",
                              "socks" };

COMPILE_ASSERT(SCHEME_MAX == SCHEME_FALLBACK,
               SCHEME_MAX_must_equal_SCHEME_FALLBACK);
COMPILE_ASSERT(arraysize(field_name) == SCHEME_MAX + 1,
               field_name_array_is_wrong_size);
COMPILE_ASSERT(arraysize(scheme_name) == SCHEME_MAX + 1,
               scheme_name_array_is_wrong_size);
COMPILE_ASSERT(SCHEME_ALL == 0, singleProxy_must_be_first_option);

}  // namespace proxy_api_constants
}  // extensions
