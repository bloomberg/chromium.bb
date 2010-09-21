// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_provider_proxy.h"

#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"

namespace chromeos {

//------------------ CrosSettingsProviderProxy: public methods -----------------

CrosSettingsProviderProxy::CrosSettingsProviderProxy() { }

void CrosSettingsProviderProxy::Set(const std::string& path,
                                    Value* in_value) {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !in_value) {
    return;
  }
  chromeos::ProxyConfigServiceImpl* config_service =
      browser->profile()->GetChromeOSProxyConfigServiceImpl();
  chromeos::ProxyConfigServiceImpl::ProxyConfig config;
  config_service->UIGetProxyConfig(&config);

  if (path == "cros.proxy.pacurl") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      GURL url(val);
      config_service->UISetProxyConfigToPACScript(url);
    }
  } else if (path == "cros.proxy.singlehttp") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(config.single_proxy, &uri);
      config_service->UISetProxyConfigToSingleProxy(
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.singlehttpport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      if (FormServerUriIfValid(config.single_proxy, val, &uri)) {
        config_service->UISetProxyConfigToSingleProxy(
            net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
      }
    }
  } else if (path == "cros.proxy.httpurl") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(config.http_proxy, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("http",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.httpsurl") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(config.https_proxy, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("https",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTPS));
    }
  } else if (path == "cros.proxy.type") {
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
  } else if (path == "cros.proxy.single") {
    bool val;
    if (in_value->GetAsBoolean(&val)) {
      if (val)
        config_service->UISetProxyConfigToSingleProxy(
            config.single_proxy.server);
      else
        config_service->UISetProxyConfigToProxyPerScheme("http",
            config.http_proxy.server);
    }
  } else if (path == "cros.proxy.ftpurl") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(config.ftp_proxy, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("ftp",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.socks") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      AppendPortIfValid(config.socks_proxy, &uri);
      config_service->UISetProxyConfigToProxyPerScheme("socks",
         net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_SOCKS4));
    }
  } else if (path == "cros.proxy.httpport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      if (FormServerUriIfValid(config.http_proxy, val, &uri)) {
        config_service->UISetProxyConfigToProxyPerScheme("http",
           net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
      }
    }
  } else if (path == "cros.proxy.httpsport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      if (FormServerUriIfValid(config.https_proxy, val, &uri)) {
        config_service->UISetProxyConfigToProxyPerScheme("https",
            net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTPS));
      }
    }
  } else if (path == "cros.proxy.ftpport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      if (FormServerUriIfValid(config.ftp_proxy, val, &uri)) {
        config_service->UISetProxyConfigToProxyPerScheme("ftp",
            net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
      }
    }
  } else if (path == "cros.proxy.socksport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri;
      if (FormServerUriIfValid(config.socks_proxy, val, &uri)) {
        config_service->UISetProxyConfigToProxyPerScheme("socks",
            net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_SOCKS5));
      }
    }
  } else if (path == "cros.proxy.ignorelist") {
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

bool CrosSettingsProviderProxy::Get(const std::string& path,
                                    Value** out_value) const {
  Browser* browser = BrowserList::GetLastActive();
  bool found = false;
  bool managed = false;
  Value* data;
  if (!browser) {
    return false;
  }
  chromeos::ProxyConfigServiceImpl* config_service =
      browser->profile()->GetChromeOSProxyConfigServiceImpl();
  chromeos::ProxyConfigServiceImpl::ProxyConfig config;
  config_service->UIGetProxyConfig(&config);

  if (path == "cros.proxy.pacurl") {
    if (config.automatic_proxy.pac_url.is_valid()) {
      data = Value::CreateStringValue(config.automatic_proxy.pac_url.spec());
      found = true;
    }
  } else if (path == "cros.proxy.singlehttp") {
    found = (data = CreateServerHostValue(config.single_proxy));
  } else if (path == "cros.proxy.singlehttpport") {
    found = (data = CreateServerPortValue(config.single_proxy));
  } else if (path == "cros.proxy.httpurl") {
    found = (data = CreateServerHostValue(config.http_proxy));
  } else if (path == "cros.proxy.httpsurl") {
    found = (data = CreateServerHostValue(config.https_proxy));
  } else if (path == "cros.proxy.type") {
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
  } else if (path == "cros.proxy.single") {
    data = Value::CreateBooleanValue(config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY);
    found = true;
  } else if (path == "cros.proxy.ftpurl") {
    found = (data = CreateServerHostValue(config.ftp_proxy));
  } else if (path == "cros.proxy.socks") {
    found = (data = CreateServerHostValue(config.socks_proxy));
  } else if (path == "cros.proxy.httpport") {
    found = (data = CreateServerPortValue(config.http_proxy));
  } else if (path == "cros.proxy.httpsport") {
    found = (data = CreateServerPortValue(config.https_proxy));
  } else if (path == "cros.proxy.ftpport") {
    found = (data = CreateServerPortValue(config.ftp_proxy));
  } else if (path == "cros.proxy.socksport") {
    found = (data = CreateServerPortValue(config.socks_proxy));
  } else if (path == "cros.proxy.ignorelist") {
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

bool CrosSettingsProviderProxy::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, "cros.proxy", true);
}

//----------------- CrosSettingsProviderProxy: private methods -----------------

void CrosSettingsProviderProxy::AppendPortIfValid(
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy,
    std::string* server_uri) {
  if (proxy.server.is_valid())
    *server_uri += ":" + proxy.server.host_port_pair().port();
}

bool CrosSettingsProviderProxy::FormServerUriIfValid(
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy,
    const std::string& port_num, std::string* server_uri) {
  if (!proxy.server.is_valid())
    return false;
  *server_uri = proxy.server.host_port_pair().host() + ":" + port_num;
  return true;
}

Value* CrosSettingsProviderProxy::CreateServerHostValue(
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const {
  return proxy.server.is_valid() ?
         Value::CreateStringValue(proxy.server.host_port_pair().host()) :
         NULL;
}

Value* CrosSettingsProviderProxy::CreateServerPortValue(
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const {
  return proxy.server.is_valid() ?
         Value::CreateIntegerValue(proxy.server.host_port_pair().port()) :
         NULL;
}

}  // namespace chromeos
