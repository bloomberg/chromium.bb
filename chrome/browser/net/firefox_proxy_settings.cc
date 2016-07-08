// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/firefox_proxy_settings.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "net/proxy/proxy_config.h"

namespace {

const char* const kNetworkProxyTypeKey = "network.proxy.type";
const char* const kHTTPProxyKey = "network.proxy.http";
const char* const kHTTPProxyPortKey = "network.proxy.http_port";
const char* const kSSLProxyKey = "network.proxy.ssl";
const char* const kSSLProxyPortKey = "network.proxy.ssl_port";
const char* const kFTPProxyKey = "network.proxy.ftp";
const char* const kFTPProxyPortKey = "network.proxy.ftp_port";
const char* const kGopherProxyKey = "network.proxy.gopher";
const char* const kGopherProxyPortKey = "network.proxy.gopher_port";
const char* const kSOCKSHostKey = "network.proxy.socks";
const char* const kSOCKSHostPortKey = "network.proxy.socks_port";
const char* const kSOCKSVersionKey = "network.proxy.socks_version";
const char* const kAutoconfigURL = "network.proxy.autoconfig_url";
const char* const kNoProxyListKey = "network.proxy.no_proxies_on";
const char* const kPrefFileName = "prefs.js";

FirefoxProxySettings::ProxyConfig IntToProxyConfig(int type) {
  switch (type) {
    case 1:
      return FirefoxProxySettings::MANUAL;
    case 2:
      return FirefoxProxySettings::AUTO_FROM_URL;
    case 4:
      return FirefoxProxySettings::AUTO_DETECT;
    case 5:
      return FirefoxProxySettings::SYSTEM;
    default:
      LOG(ERROR) << "Unknown Firefox proxy config type: " << type;
      return FirefoxProxySettings::NO_PROXY;
  }
}

FirefoxProxySettings::SOCKSVersion IntToSOCKSVersion(int type) {
  switch (type) {
    case 4:
      return FirefoxProxySettings::V4;
    case 5:
      return FirefoxProxySettings::V5;
    default:
      LOG(ERROR) << "Unknown Firefox proxy config type: " << type;
      return FirefoxProxySettings::UNKNONW;
  }
}

// Parses the prefs found in the file |pref_file| and puts the key/value pairs
// in |prefs|. Keys are strings, and values can be strings, booleans or
// integers.  Returns true if it succeeded, false otherwise (in which case
// |prefs| is not filled).
// Note: for strings, only valid UTF-8 string values are supported. If a
// key/pair is not valid UTF-8, it is ignored and will not appear in |prefs|.
bool ParsePrefFile(const base::FilePath& pref_file,
                   base::DictionaryValue* prefs) {
  // The string that is before a pref key.
  const std::string kUserPrefString = "user_pref(\"";
  std::string contents;
  if (!base::ReadFileToString(pref_file, &contents))
    return false;

  for (const std::string& line :
       base::SplitString(contents, "\n", base::KEEP_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    size_t start_key = line.find(kUserPrefString);
    if (start_key == std::string::npos)
      continue;  // Could be a comment or a blank line.
    start_key += kUserPrefString.length();
    size_t stop_key = line.find('"', start_key);
    if (stop_key == std::string::npos) {
      LOG(ERROR) << "Invalid key found in Firefox pref file '" <<
          pref_file.value() << "' line is '" << line << "'.";
      continue;
    }
    std::string key = line.substr(start_key, stop_key - start_key);
    size_t start_value = line.find(',', stop_key + 1);
    if (start_value == std::string::npos) {
      LOG(ERROR) << "Invalid value found in Firefox pref file '" <<
          pref_file.value() << "' line is '" << line << "'.";
      continue;
    }
    size_t stop_value = line.find(");", start_value + 1);
    if (stop_value == std::string::npos) {
      LOG(ERROR) << "Invalid value found in Firefox pref file '" <<
          pref_file.value() << "' line is '" << line << "'.";
      continue;
    }
    std::string value = line.substr(start_value + 1,
                                    stop_value - start_value - 1);
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
    // Value could be a boolean.
    bool is_value_true = base::LowerCaseEqualsASCII(value, "true");
    if (is_value_true || base::LowerCaseEqualsASCII(value, "false")) {
      prefs->SetBoolean(key, is_value_true);
      continue;
    }

    // Value could be a string.
    if (value.size() >= 2U && value[0] == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
      // ValueString only accept valid UTF-8.  Simply ignore that entry if it is
      // not UTF-8.
      if (base::IsStringUTF8(value))
        prefs->SetString(key, value);
      else
        VLOG(1) << "Non UTF8 value for key " << key << ", ignored.";
      continue;
    }

    // Or value could be an integer.
    int int_value = 0;
    if (base::StringToInt(value, &int_value)) {
      prefs->SetInteger(key, int_value);
      continue;
    }

    LOG(ERROR) << "Invalid value found in Firefox pref file '"
               << pref_file.value() << "' value is '" << value << "'.";
  }
  return true;
}

}  // namespace

FirefoxProxySettings::FirefoxProxySettings() {
  Reset();
}

FirefoxProxySettings::~FirefoxProxySettings() {
}

void FirefoxProxySettings::Reset() {
  config_type_ = NO_PROXY;
  http_proxy_.clear();
  http_proxy_port_ = 0;
  ssl_proxy_.clear();
  ssl_proxy_port_ = 0;
  ftp_proxy_.clear();
  ftp_proxy_port_ = 0;
  gopher_proxy_.clear();
  gopher_proxy_port_ = 0;
  socks_host_.clear();
  socks_port_ = 0;
  socks_version_ = UNKNONW;
  proxy_bypass_list_.clear();
  autoconfig_url_.clear();
}

// static
bool FirefoxProxySettings::GetSettings(FirefoxProxySettings* settings) {
  DCHECK(settings);
  settings->Reset();

  base::FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return false;
  base::FilePath pref_file = profile_path.AppendASCII(kPrefFileName);
  return GetSettingsFromFile(pref_file, settings);
}

bool FirefoxProxySettings::ToProxyConfig(net::ProxyConfig* config) {
  switch (config_type()) {
    case NO_PROXY:
      *config = net::ProxyConfig::CreateDirect();
      return true;
    case AUTO_DETECT:
      *config = net::ProxyConfig::CreateAutoDetect();
      return true;
    case AUTO_FROM_URL:
      *config = net::ProxyConfig::CreateFromCustomPacURL(
          GURL(autoconfig_url()));
      return true;
    case SYSTEM:
      // Can't convert this directly to a ProxyConfig.
      return false;
    case MANUAL:
      // Handled outside of the switch (since it is a lot of code.)
      break;
    default:
      NOTREACHED();
      return false;
  }

  // The rest of this funciton is for handling the MANUAL case.
  DCHECK_EQ(MANUAL, config_type());

  *config = net::ProxyConfig();
  config->proxy_rules().type =
      net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;

  if (!http_proxy().empty()) {
    config->proxy_rules().proxies_for_http.SetSingleProxyServer(
        net::ProxyServer(
            net::ProxyServer::SCHEME_HTTP,
            net::HostPortPair(http_proxy(), http_proxy_port())));
  }

  if (!ftp_proxy().empty()) {
    config->proxy_rules().proxies_for_ftp.SetSingleProxyServer(
        net::ProxyServer(
            net::ProxyServer::SCHEME_HTTP,
            net::HostPortPair(ftp_proxy(), ftp_proxy_port())));
  }

  if (!ssl_proxy().empty()) {
    config->proxy_rules().proxies_for_https.SetSingleProxyServer(
        net::ProxyServer(
            net::ProxyServer::SCHEME_HTTP,
            net::HostPortPair(ssl_proxy(), ssl_proxy_port())));
  }

  if (!socks_host().empty()) {
    net::ProxyServer::Scheme proxy_scheme = V5 == socks_version() ?
        net::ProxyServer::SCHEME_SOCKS5 : net::ProxyServer::SCHEME_SOCKS4;

    config->proxy_rules().fallback_proxies.SetSingleProxyServer(
        net::ProxyServer(
            proxy_scheme,
            net::HostPortPair(socks_host(), socks_port())));
  }

  config->proxy_rules().bypass_rules.ParseFromStringUsingSuffixMatching(
      base::JoinString(proxy_bypass_list_, ";"));

  return true;
}

// static
bool FirefoxProxySettings::GetSettingsFromFile(const base::FilePath& pref_file,
                                               FirefoxProxySettings* settings) {
  base::DictionaryValue dictionary;
  if (!ParsePrefFile(pref_file, &dictionary))
    return false;

  int proxy_type = 0;
  if (!dictionary.GetInteger(kNetworkProxyTypeKey, &proxy_type))
    return true;  // No type means no proxy.

  settings->config_type_ = IntToProxyConfig(proxy_type);
  if (settings->config_type_ == AUTO_FROM_URL) {
    if (!dictionary.GetStringASCII(kAutoconfigURL,
                                   &(settings->autoconfig_url_))) {
      LOG(ERROR) << "Failed to retrieve Firefox proxy autoconfig URL";
    }
    return true;
  }

  if (settings->config_type_ == MANUAL) {
    if (!dictionary.GetStringASCII(kHTTPProxyKey, &(settings->http_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy HTTP host";
    if (!dictionary.GetInteger(kHTTPProxyPortKey,
                               &(settings->http_proxy_port_))) {
      LOG(ERROR) << "Failed to retrieve Firefox proxy HTTP port";
    }
    if (!dictionary.GetStringASCII(kSSLProxyKey, &(settings->ssl_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy SSL host";
    if (!dictionary.GetInteger(kSSLProxyPortKey, &(settings->ssl_proxy_port_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy SSL port";
    if (!dictionary.GetStringASCII(kFTPProxyKey, &(settings->ftp_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy FTP host";
    if (!dictionary.GetInteger(kFTPProxyPortKey, &(settings->ftp_proxy_port_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy SSL port";
    if (!dictionary.GetStringASCII(kGopherProxyKey, &(settings->gopher_proxy_)))
      LOG(ERROR) << "Failed to retrieve Firefox proxy gopher host";
    if (!dictionary.GetInteger(kGopherProxyPortKey,
                               &(settings->gopher_proxy_port_))) {
      LOG(ERROR) << "Failed to retrieve Firefox proxy gopher port";
    }
    if (!dictionary.GetStringASCII(kSOCKSHostKey, &(settings->socks_host_)))
      LOG(ERROR) << "Failed to retrieve Firefox SOCKS host";
    if (!dictionary.GetInteger(kSOCKSHostPortKey, &(settings->socks_port_)))
      LOG(ERROR) << "Failed to retrieve Firefox SOCKS port";
    int socks_version;
    if (dictionary.GetInteger(kSOCKSVersionKey, &socks_version))
      settings->socks_version_ = IntToSOCKSVersion(socks_version);

    std::string proxy_bypass;
    if (dictionary.GetStringASCII(kNoProxyListKey, &proxy_bypass) &&
        !proxy_bypass.empty()) {
      base::StringTokenizer string_tok(proxy_bypass, ",");
      while (string_tok.GetNext()) {
        std::string token = string_tok.token();
        base::TrimWhitespaceASCII(token, base::TRIM_ALL, &token);
        if (!token.empty())
          settings->proxy_bypass_list_.push_back(token);
      }
    }
  }
  return true;
}
