// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/onc_utils.h"

#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_server.h"

namespace chromeos {
namespace onc {

namespace {

net::ProxyServer ConvertOncProxyLocationToHostPort(
    net::ProxyServer::Scheme default_proxy_scheme,
    const base::DictionaryValue& onc_proxy_location) {
  std::string host;
  onc_proxy_location.GetStringWithoutPathExpansion(onc::proxy::kHost, &host);
  // Parse |host| according to the format [<scheme>"://"]<server>[":"<port>].
  net::ProxyServer proxy_server =
      net::ProxyServer::FromURI(host, default_proxy_scheme);
  int port = 0;
  onc_proxy_location.GetIntegerWithoutPathExpansion(onc::proxy::kPort, &port);

  // Replace the port parsed from |host| by the provided |port|.
  return net::ProxyServer(
      proxy_server.scheme(),
      net::HostPortPair(proxy_server.host_port_pair().host(),
                        static_cast<uint16>(port)));
}

void AppendProxyServerForScheme(
    const base::DictionaryValue& onc_manual,
    const std::string& onc_scheme,
    std::string* spec) {
  const base::DictionaryValue* onc_proxy_location = NULL;
  if (!onc_manual.GetDictionaryWithoutPathExpansion(onc_scheme,
                                                    &onc_proxy_location)) {
    return;
  }

  net::ProxyServer::Scheme default_proxy_scheme = net::ProxyServer::SCHEME_HTTP;
  std::string url_scheme;
  if (onc_scheme == proxy::kFtp) {
    url_scheme = "ftp";
  } else if (onc_scheme == proxy::kHttp) {
    url_scheme = "http";
  } else if (onc_scheme == proxy::kHttps) {
    url_scheme = "https";
  } else if (onc_scheme == proxy::kSocks) {
    default_proxy_scheme = net::ProxyServer::SCHEME_SOCKS4;
    url_scheme = "socks";
  } else {
    NOTREACHED();
  }

  net::ProxyServer proxy_server = ConvertOncProxyLocationToHostPort(
      default_proxy_scheme, *onc_proxy_location);

  ProxyConfigServiceImpl::ProxyConfig::EncodeAndAppendProxyServer(
      url_scheme, proxy_server, spec);
}

net::ProxyBypassRules ConvertOncExcludeDomainsToBypassRules(
    const base::ListValue& onc_exclude_domains) {
  net::ProxyBypassRules rules;
  for (base::ListValue::const_iterator it = onc_exclude_domains.begin();
       it != onc_exclude_domains.end(); ++it) {
    std::string rule;
    (*it)->GetAsString(&rule);
    rules.AddRuleFromString(rule);
  }
  return rules;
}

}  // namespace

scoped_ptr<base::DictionaryValue> ConvertOncProxySettingsToProxyConfig(
    const base::DictionaryValue& onc_proxy_settings) {
  std::string type;
  onc_proxy_settings.GetStringWithoutPathExpansion(proxy::kType, &type);
  scoped_ptr<DictionaryValue> proxy_dict;

  if (type == proxy::kDirect) {
    proxy_dict.reset(ProxyConfigDictionary::CreateDirect());
  } else if (type == proxy::kWPAD) {
    proxy_dict.reset(ProxyConfigDictionary::CreateAutoDetect());
  } else if (type == proxy::kPAC) {
    std::string pac_url;
    onc_proxy_settings.GetStringWithoutPathExpansion(proxy::kPAC, &pac_url);
    GURL url(pac_url);
    DCHECK(url.is_valid())
        << "PAC field is invalid for this ProxySettings.Type";
    proxy_dict.reset(ProxyConfigDictionary::CreatePacScript(url.spec(),
                                                            false));
  } else if (type == proxy::kManual) {
    const base::DictionaryValue* manual_dict = NULL;
    onc_proxy_settings.GetDictionaryWithoutPathExpansion(proxy::kManual,
                                                         &manual_dict);
    std::string manual_spec;
    AppendProxyServerForScheme(*manual_dict, proxy::kFtp, &manual_spec);
    AppendProxyServerForScheme(*manual_dict, proxy::kHttp, &manual_spec);
    AppendProxyServerForScheme(*manual_dict, proxy::kSocks, &manual_spec);
    AppendProxyServerForScheme(*manual_dict, proxy::kHttps, &manual_spec);

    const base::ListValue* exclude_domains = NULL;
    net::ProxyBypassRules bypass_rules;
    if (onc_proxy_settings.GetListWithoutPathExpansion(proxy::kExcludeDomains,
                                                       &exclude_domains)) {
      bypass_rules.AssignFrom(
          ConvertOncExcludeDomainsToBypassRules(*exclude_domains));
    }
    proxy_dict.reset(ProxyConfigDictionary::CreateFixedServers(
        manual_spec, bypass_rules.ToString()));
  } else {
    NOTREACHED();
  }
  return proxy_dict.Pass();
}

namespace {

void TranslateClientCertType(const std::string& client_cert_type,
                             NetworkUIData* ui_data) {
  ClientCertType type;
  if (client_cert_type == certificate::kNone) {
    type = CLIENT_CERT_TYPE_NONE;
  } else if (client_cert_type == certificate::kRef) {
    type = CLIENT_CERT_TYPE_REF;
  } else if (client_cert_type == certificate::kPattern) {
    type = CLIENT_CERT_TYPE_PATTERN;
  } else {
    type = CLIENT_CERT_TYPE_NONE;
    NOTREACHED();
  }

  ui_data->set_certificate_type(type);
}

void TranslateCertificatePattern(const base::DictionaryValue& onc_object,
                                 NetworkUIData* ui_data) {
  CertificatePattern pattern;
  bool success = pattern.CopyFromDictionary(onc_object);
  DCHECK(success);
  ui_data->set_certificate_pattern(pattern);
}

void TranslateEAP(const base::DictionaryValue& eap,
                  NetworkUIData* ui_data) {
  std::string client_cert_type;
  if (eap.GetStringWithoutPathExpansion(eap::kClientCertType,
                                        &client_cert_type)) {
    TranslateClientCertType(client_cert_type, ui_data);
  }
}

void TranslateIPsec(const base::DictionaryValue& ipsec,
                    NetworkUIData* ui_data) {
  std::string client_cert_type;
  if (ipsec.GetStringWithoutPathExpansion(vpn::kClientCertType,
                                          &client_cert_type)) {
    TranslateClientCertType(client_cert_type, ui_data);
  }
}

void TranslateOpenVPN(const base::DictionaryValue& openvpn,
                      NetworkUIData* ui_data) {
  std::string client_cert_type;
  if (openvpn.GetStringWithoutPathExpansion(vpn::kClientCertType,
                                            &client_cert_type)) {
    TranslateClientCertType(client_cert_type, ui_data);
  }
}

void TranslateONCHierarchy(const OncValueSignature& signature,
                           const base::DictionaryValue& onc_object,
                           NetworkUIData* ui_data) {
  if (&signature == &kCertificatePatternSignature)
    TranslateCertificatePattern(onc_object, ui_data);
  else if (&signature == &kEAPSignature)
    TranslateEAP(onc_object, ui_data);
  else if (&signature == &kIPsecSignature)
    TranslateIPsec(onc_object, ui_data);
  else if (&signature == &kOpenVPNSignature)
    TranslateOpenVPN(onc_object, ui_data);

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(onc_object); it.HasNext();
       it.Advance()) {
    const base::DictionaryValue* inner_object;
    if (!it.value().GetAsDictionary(&inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());

    TranslateONCHierarchy(*field_signature->value_signature, *inner_object,
                          ui_data);
  }
}

}  // namespace

scoped_ptr<NetworkUIData> CreateUIData(
    ONCSource onc_source,
    const base::DictionaryValue& onc_network) {
  scoped_ptr<NetworkUIData> ui_data(new NetworkUIData());
  TranslateONCHierarchy(kNetworkConfigurationSignature, onc_network,
                        ui_data.get());

  ui_data->set_onc_source(onc_source);

  return ui_data.Pass();
}

}  // namespace onc
}  // namespace chromeos
