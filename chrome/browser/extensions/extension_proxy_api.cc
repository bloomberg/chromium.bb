// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_proxy_api.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"

namespace {

// The scheme for which to use the proxy, not of the proxy URI itself.
enum {
  SCHEME_ALL = 0,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FTP,
  SCHEME_SOCKS,
  SCHEME_MAX = SCHEME_SOCKS  // Keep this value up to date.
};

// The names of the JavaScript properties to extract from the args_.
// These must be kept in sync with the SCHEME_* constants.
const char* field_name[] = { "singleProxy",
                             "proxyForHttp",
                             "proxyForHttps",
                             "proxyForFtp",
                             "socksProxy" };

// The names of the schemes to be used to build the preference value string.
// These must be kept in sync with the SCHEME_* constants.
const char* scheme_name[] = { "*error*",
                              "http",
                              "https",
                              "ftp",
                              "socks" };

}  // namespace

COMPILE_ASSERT(SCHEME_MAX == SCHEME_SOCKS, SCHEME_MAX_must_equal_SCHEME_SOCKS);
COMPILE_ASSERT(arraysize(field_name) == SCHEME_MAX + 1,
               field_name_array_is_wrong_size);
COMPILE_ASSERT(arraysize(scheme_name) == SCHEME_MAX + 1,
               scheme_name_array_is_wrong_size);

bool UseCustomProxySettingsFunction::RunImpl() {
  DictionaryValue* proxy_config;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &proxy_config));

  DictionaryValue* proxy_rules;
  EXTENSION_FUNCTION_VALIDATE(proxy_config->GetDictionary("rules",
      &proxy_rules));

  // Local data into which the parameters will be parsed. has_proxy describes
  // whether a setting was found for the scheme; proxy_dict holds the
  // DictionaryValues which in turn contain proxy server descriptions, and
  // proxy_server holds ProxyServer structs containing those descriptions.
  bool has_proxy[SCHEME_MAX + 1];
  DictionaryValue* proxy_dict[SCHEME_MAX + 1];
  ProxyServer proxy_server[SCHEME_MAX + 1];

  // Looking for all possible proxy types is inefficient if we have a
  // singleProxy that will supersede per-URL proxies, but it's worth it to keep
  // the code simple and extensible.
  for (size_t i = 0; i <= SCHEME_MAX; ++i) {
    has_proxy[i] = proxy_rules->GetDictionary(field_name[i], &proxy_dict[i]);
    if (has_proxy[i]) {
      if (!GetProxyServer(proxy_dict[i], &proxy_server[i]))
        return false;
    }
  }

  // A single proxy supersedes individual HTTP, HTTPS, and FTP proxies.
  if (has_proxy[SCHEME_ALL]) {
    proxy_server[SCHEME_HTTP] = proxy_server[SCHEME_ALL];
    proxy_server[SCHEME_HTTPS] = proxy_server[SCHEME_ALL];
    proxy_server[SCHEME_FTP] = proxy_server[SCHEME_ALL];
    has_proxy[SCHEME_HTTP] = true;
    has_proxy[SCHEME_HTTPS] = true;
    has_proxy[SCHEME_FTP] = true;
    has_proxy[SCHEME_ALL] = false;
  }

  // TODO(pamg): Ensure that if a value is empty, that means "don't use a proxy
  // for this scheme".

  // Build the proxy preference string.
  std::string proxy_pref;
  for (size_t i = 0; i <= SCHEME_MAX; ++i) {
    if (has_proxy[i]) {
      // http=foopy:4010;ftp=socks://foopy2:80
      if (!proxy_pref.empty())
        proxy_pref.append(";");
      proxy_pref.append(scheme_name[i]);
      proxy_pref.append("=");
      proxy_pref.append(proxy_server[i].scheme);
      proxy_pref.append("://");
      proxy_pref.append(proxy_server[i].host);
      if (proxy_server[i].port != ProxyServer::INVALID_PORT) {
        proxy_pref.append(":");
        proxy_pref.append(StringPrintf("%d", proxy_server[i].port));
      }
    }
  }

  ExtensionPrefStore::ExtensionPrefDetails details =
      std::make_pair(GetExtension(),
                     std::make_pair(prefs::kProxyServer,
                                    Value::CreateStringValue(proxy_pref)));

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PREF_CHANGED,
      Source<Profile>(profile_),
      Details<ExtensionPrefStore::ExtensionPrefDetails>(&details));

  return true;
}

bool UseCustomProxySettingsFunction::GetProxyServer(
    const DictionaryValue* dict, ProxyServer* proxy_server) {
  dict->GetString("scheme", &proxy_server->scheme);
  EXTENSION_FUNCTION_VALIDATE(dict->GetString("host", &proxy_server->host));
  dict->GetInteger("port", &proxy_server->port);
  return true;
}
