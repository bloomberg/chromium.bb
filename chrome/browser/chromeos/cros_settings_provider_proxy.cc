// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings_provider_proxy.h"

#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/proxy_config_service.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/profile.h"
#include "net/proxy/proxy_config.h"

namespace chromeos {

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
      uri += ":";
      uri += config.single_proxy.server.host_port_pair().port();
      config_service->UISetProxyConfigToSingleProxy(
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.singlehttpport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = config.single_proxy.server.host_port_pair().host();
      uri += ":";
      uri += val;
      config_service->UISetProxyConfigToSingleProxy(
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.httpurl") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      uri += ":";
      uri += config.http_proxy.server.host_port_pair().port();
      config_service->UISetProxyConfigToProxyPerScheme("http",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.httpsurl") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      uri += config.https_proxy.server.host_port_pair().port();
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
        config_service->UISetProxyConfigToProxyPerScheme("http",
            net::ProxyServer());
      } else {
        config_service->UISetProxyConfigToDirect();
      }
    }
  } else if (path == "cros.proxy.single") {
    bool val;
    if (in_value->GetAsBoolean(&val)) {
      if (val)
        config_service->UISetProxyConfigToSingleProxy(config.http_proxy.server);
      else
        config_service->UISetProxyConfigToProxyPerScheme("http",
            config.http_proxy.server);
    }
  } else if (path == "cros.proxy.ftp") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      uri += config.ftp_proxy.server.host_port_pair().port();
      config_service->UISetProxyConfigToProxyPerScheme("ftp",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.socks") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = val;
      uri += config.socks_proxy.server.host_port_pair().port();
      config_service->UISetProxyConfigToProxyPerScheme("socks",
         net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_SOCKS4));
    }
  } else if (path == "cros.proxy.httpport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = config.http_proxy.server.host_port_pair().host() +
                          ":" + val;
      config_service->UISetProxyConfigToProxyPerScheme("http",
         net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.httpsport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = config.https_proxy.server.host_port_pair().host();
      uri += ":";
      uri += val;
      config_service->UISetProxyConfigToProxyPerScheme("https",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTPS));
    }
  } else if (path == "cros.proxy.ftpport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = config.ftp_proxy.server.host_port_pair().host();
      uri += ":";
      uri += val;
      config_service->UISetProxyConfigToProxyPerScheme("ftp",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_HTTP));
    }
  } else if (path == "cros.proxy.socksport") {
    std::string val;
    if (in_value->GetAsString(&val)) {
      std::string uri = config.socks_proxy.server.host_port_pair().host();
      uri += ":";
      uri += val;
      config_service->UISetProxyConfigToProxyPerScheme("socks",
          net::ProxyServer::FromURI(uri, net::ProxyServer::SCHEME_SOCKS5));
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
    if (config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_PAC_SCRIPT) {
      data = Value::CreateStringValue(
          config.automatic_proxy.pac_url.spec());
      found = true;
    }
  } else if (path == "cros.proxy.singlehttp") {
    data = Value::CreateStringValue(
        config.single_proxy.server.host_port_pair().host());
    found = true;
  } else if (path == "cros.proxy.singlehttpport") {
    data = Value::CreateIntegerValue(
        config.single_proxy.server.host_port_pair().port());
    found = true;
  } else if (path == "cros.proxy.httpurl") {
    data = Value::CreateStringValue(
        config.http_proxy.server.host_port_pair().host());
    found = true;
  } else if (path == "cros.proxy.httpsurl") {
    data = Value::CreateStringValue(
        config.https_proxy.server.host_port_pair().host());
    found = true;
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
    if (config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY) {
      data = Value::CreateBooleanValue(true);
    } else {
      data = Value::CreateBooleanValue(false);
    }
    found = true;
  } else if (path == "cros.proxy.http") {
    net::ProxyServer& server = config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY ?
        config.single_proxy.server : config.http_proxy.server;
    data = Value::CreateStringValue(server.host_port_pair().host());
    found = true;
  } else if (path == "cros.proxy.https") {
    data = Value::CreateStringValue(
        config.https_proxy.server.host_port_pair().host());
    found = true;
  } else if (path == "cros.proxy.ftp") {
    data = Value::CreateStringValue(
        config.ftp_proxy.server.host_port_pair().host());
    found = true;
  } else if (path == "cros.proxy.socks") {
    data = Value::CreateStringValue(
        config.socks_proxy.server.host_port_pair().host());
    found = true;
  } else if (path == "cros.proxy.httpport") {
    net::ProxyServer& server = config.mode ==
        chromeos::ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY ?
        config.single_proxy.server : config.http_proxy.server;
    data = Value::CreateIntegerValue(server.host_port_pair().port());
    found = true;
  } else if (path == "cros.proxy.httpsport") {
    data = Value::CreateIntegerValue(
        config.https_proxy.server.host_port_pair().port());
    found = true;
  } else if (path == "cros.proxy.ftpport") {
    data = Value::CreateIntegerValue(
        config.ftp_proxy.server.host_port_pair().port());
    found = true;
  } else if (path == "cros.proxy.socksport") {
    data = Value::CreateIntegerValue(
        config.socks_proxy.server.host_port_pair().port());
    found = true;
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

}  // namespace chromeos
