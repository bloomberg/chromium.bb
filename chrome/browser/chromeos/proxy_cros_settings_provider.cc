// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_cros_settings_provider.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"

namespace chromeos {

static const char kProxyPacUrl[]         = "cros.session.proxy.pacurl";
static const char kProxySingleHttp[]     = "cros.session.proxy.singlehttp";
static const char kProxySingleHttpPort[] = "cros.session.proxy.singlehttpport";
static const char kProxyHttpUrl[]        = "cros.session.proxy.httpurl";
static const char kProxyHttpPort[]       = "cros.session.proxy.httpport";
static const char kProxyHttpsUrl[]       = "cros.session.proxy.httpsurl";
static const char kProxyHttpsPort[]      = "cros.session.proxy.httpsport";
static const char kProxyType[]           = "cros.session.proxy.type";
static const char kProxySingle[]         = "cros.session.proxy.single";
static const char kProxyFtpUrl[]         = "cros.session.proxy.ftpurl";
static const char kProxyFtpPort[]        = "cros.session.proxy.ftpport";
static const char kProxySocks[]          = "cros.session.proxy.socks";
static const char kProxySocksPort[]      = "cros.session.proxy.socksport";
static const char kProxyIgnoreList[]     = "cros.session.proxy.ignorelist";

//------------------ ProxyCrosSettingsProvider: public methods -----------------

ProxyCrosSettingsProvider::ProxyCrosSettingsProvider() { }

void ProxyCrosSettingsProvider::DoSet(const std::string& path,
                                      Value* in_value) {
  if (!in_value) {
    return;
  }

  // Keep whatever user inputs so that we could use it later.
  SetCache(path, in_value);

  chromeos::ProxyConfigServiceImpl* config_service = GetConfigService();
  // Don't persist settings to device for guest session.
  config_service->UISetPersistToDevice(
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession));
  // Retrieve proxy config.
  chromeos::ProxyConfigServiceImpl::ProxyConfig config;
  config_service->UIGetProxyConfig(&config);

  if (path == kProxyPacUrl) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      GURL url(val);
      config_service->UISetProxyConfigToPACScript(url);
    }
  } else if (path == kProxySingleHttp) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(kProxySingleHttpPort, &uri);
      config_service->UISetProxyConfigToSingleProxy(
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxySingleHttpPort) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      FormServerUriIfValid(kProxySingleHttp, val, &uri);
      config_service->UISetProxyConfigToSingleProxy(
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxyHttpUrl) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(kProxyHttpPort, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("http",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxyHttpPort) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      FormServerUriIfValid(kProxyHttpUrl, val, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("http",
         net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxyHttpsUrl) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(kProxyHttpsPort, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("https",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxyHttpsPort) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      FormServerUriIfValid(kProxyHttpsUrl, val, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("https",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxyType) {
    int val;
    if (in_value->GetAsInteger(&val)) {
      if (val == 3) {
        if (config.automatic_proxy.pac_url.is_valid())
          config_service->UISetProxyConfigToPACScript(
              config.automatic_proxy.pac_url);
        else
          config_service->UISetProxyConfigToAutoDetect();
      } else if (val == 2) {
        if (config.single_proxy.server.is_valid()) {
          config_service->UISetProxyConfigToSingleProxy(
              config.single_proxy.server);
        } else {
          bool set_config = false;
          if (config.http_proxy.server.is_valid()) {
            config_service->UISetProxyConfigToProxyPerScheme("http",
                config.http_proxy.server);
            set_config = true;
          }
          if (config.https_proxy.server.is_valid()) {
            config_service->UISetProxyConfigToProxyPerScheme("https",
                config.https_proxy.server);
            set_config = true;
          }
          if (config.ftp_proxy.server.is_valid()) {
            config_service->UISetProxyConfigToProxyPerScheme("ftp",
                config.ftp_proxy.server);
            set_config = true;
          }
          if (config.socks_proxy.server.is_valid()) {
            config_service->UISetProxyConfigToProxyPerScheme("socks",
                config.socks_proxy.server);
            set_config = true;
          }
          if (!set_config) {
            config_service->UISetProxyConfigToProxyPerScheme("http",
                net::ProxyServer());
          }
        }
      } else {
        config_service->UISetProxyConfigToDirect();
      }
    }
  } else if (path == kProxySingle) {
    bool val;
    if (in_value->GetAsBoolean(&val)) {
      if (val)
        config_service->UISetProxyConfigToSingleProxy(
            config.single_proxy.server);
      else
        config_service->UISetProxyConfigToProxyPerScheme("http",
            config.http_proxy.server);
    }
  } else if (path == kProxyFtpUrl) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(kProxyFtpPort, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("ftp",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxyFtpPort) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      FormServerUriIfValid(kProxyFtpUrl, val, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("ftp",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == kProxySocks) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(kProxySocksPort, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("socks",
          net::ProxyServer::FromURI(uri,
              StartsWithASCII(uri, "socks4://", false) ?
                  net::ProxyServer::SCHEME_SOCKS4 :
                  net::ProxyServer::SCHEME_SOCKS5));
    }
  } else if (path == kProxySocksPort) {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      FormServerUriIfValid(kProxySocks, val, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("socks",
          net::ProxyServer::FromURI(uri,
              StartsWithASCII(uri, "socks4://", false) ?
                  net::ProxyServer::SCHEME_SOCKS4 :
                  net::ProxyServer::SCHEME_SOCKS5));
    }
  } else if (path == kProxyIgnoreList) {
    net::ProxyBypassRules bypass_rules;
    if (in_value->GetType() == Value::TYPE_LIST) {
      const ListValue* list_value = static_cast<const ListValue*>(in_value);
      for (size_t x = 0; x < list_value->GetSize(); x++) {
        std::string val;
        if (list_value->GetString(x, &val)) {
          bypass_rules.AddRuleFromString(val);
        }
      }
      config_service->UISetProxyConfigBypassRules(bypass_rules);
    }
  }
}

bool ProxyCrosSettingsProvider::Get(const std::string& path,
                                    Value** out_value) const {
  bool found = false;
  bool managed = false;
  Value* data;
  chromeos::ProxyConfigServiceImpl* config_service = GetConfigService();
  chromeos::ProxyConfigServiceImpl::ProxyConfig config;
  config_service->UIGetProxyConfig(&config);

  if (path == kProxyPacUrl) {
    if (config.automatic_proxy.pac_url.is_valid()) {
      data = Value::CreateStringValue(config.automatic_proxy.pac_url.spec());
      found = true;
    }
  } else if (path == kProxySingleHttp) {
    found = (data = CreateServerHostValue(config.single_proxy));
  } else if (path == kProxySingleHttpPort) {
    found = (data = CreateServerPortValue(config.single_proxy));
  } else if (path == kProxyHttpUrl) {
    found = (data = CreateServerHostValue(config.http_proxy));
  } else if (path == kProxyHttpsUrl) {
    found = (data = CreateServerHostValue(config.https_proxy));
  } else if (path == kProxyType) {
    if (config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_AUTO_DETECT ||
        config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_PAC_SCRIPT) {
      data = Value::CreateIntegerValue(3);
    } else if (config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY ||
        config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME) {
      data = Value::CreateIntegerValue(2);
    } else {
      data = Value::CreateIntegerValue(1);
    }
    found = true;
  } else if (path == kProxySingle) {
    data = Value::CreateBooleanValue(config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY);
    found = true;
  } else if (path == kProxyFtpUrl) {
    found = (data = CreateServerHostValue(config.ftp_proxy));
  } else if (path == kProxySocks) {
    found = (data = CreateServerHostValue(config.socks_proxy));
  } else if (path == kProxyHttpPort) {
    found = (data = CreateServerPortValue(config.http_proxy));
  } else if (path == kProxyHttpsPort) {
    found = (data = CreateServerPortValue(config.https_proxy));
  } else if (path == kProxyFtpPort) {
    found = (data = CreateServerPortValue(config.ftp_proxy));
  } else if (path == kProxySocksPort) {
    found = (data = CreateServerPortValue(config.socks_proxy));
  } else if (path == kProxyIgnoreList) {
    ListValue* list =  new ListValue();
    net::ProxyBypassRules::RuleList bypass_rules = config.bypass_rules.rules();
    for (size_t x = 0; x < bypass_rules.size(); x++) {
      list->Append(Value::CreateStringValue(bypass_rules[x]->ToString()));
    }
    *out_value = list;
    return true;
  }
  if (found) {
    DictionaryValue* dict = new DictionaryValue;
    dict->Set("value", data);
    dict->SetBoolean("managed", managed);
    *out_value = dict;
    return true;
  } else {
    *out_value = NULL;
    return false;
  }
}

bool ProxyCrosSettingsProvider::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, "cros.session.proxy", true);
}

//----------------- ProxyCrosSettingsProvider: private methods -----------------

chromeos::ProxyConfigServiceImpl*
    ProxyCrosSettingsProvider::GetConfigService() const {
  Browser* browser = BrowserList::GetLastActive();
  // browser is NULL at OOBE/login stage.
  Profile* profile = browser ?
      browser->profile() :
      ProfileManager::GetDefaultProfile();
  return profile->GetChromeOSProxyConfigServiceImpl();
}

void ProxyCrosSettingsProvider::AppendPortIfValid(
    const char* port_cache_key,
    std::string* server_uri) {
  std::string port;
  if (!server_uri->empty() && cache_.GetString(port_cache_key, &port) &&
      !port.empty()) {
    *server_uri += ":" + port;
  }
}

void ProxyCrosSettingsProvider::FormServerUriIfValid(
    const char* host_cache_key,
    const std::string& port_num, std::string* server_uri) {
  if (cache_.GetString(host_cache_key, server_uri) && !server_uri->empty() &&
      !port_num.empty())
    *server_uri += ":" + port_num;
}

Value* ProxyCrosSettingsProvider::CreateServerHostValue(
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const {
  return proxy.server.is_valid() ?
         Value::CreateStringValue(proxy.server.host_port_pair().host()) :
         NULL;
}

Value* ProxyCrosSettingsProvider::CreateServerPortValue(
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const {
  return proxy.server.is_valid() ?
         Value::CreateIntegerValue(proxy.server.host_port_pair().port()) :
         NULL;
}

void ProxyCrosSettingsProvider::SetCache(const std::string& key,
    const Value* value) {
  cache_.Set(key, value->DeepCopy());
}

}  // namespace chromeos
