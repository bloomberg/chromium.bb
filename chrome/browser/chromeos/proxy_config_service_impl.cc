// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <ostream>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "content/browser/browser_thread.h"
#include "content/common/json_value_serializer.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

const char* SourceToString(ProxyConfigServiceImpl::ProxyConfig::Source source) {
  switch (source) {
    case ProxyConfigServiceImpl::ProxyConfig::SOURCE_NONE:
      return "SOURCE_NONE";
    case ProxyConfigServiceImpl::ProxyConfig::SOURCE_POLICY:
      return "SOURCE_POLICY";
    case ProxyConfigServiceImpl::ProxyConfig::SOURCE_OWNER:
      return "SOURCE_OWNER";
  }
  NOTREACHED() << "Unrecognized source type";
  return "";
}

std::ostream& operator<<(std::ostream& out,
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) {
  out << "  " << SourceToString(proxy.source) << "\n"
      << "  server: " << (proxy.server.is_valid() ? proxy.server.ToURI() : "")
      << "\n";
  return out;
}

std::ostream& operator<<(std::ostream& out,
    const ProxyConfigServiceImpl::ProxyConfig& config) {
  switch (config.mode) {
    case ProxyConfigServiceImpl::ProxyConfig::MODE_DIRECT:
      out << "Direct connection:\n  "
          << SourceToString(config.automatic_proxy.source) << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_AUTO_DETECT:
      out << "Auto detection:\n  "
          << SourceToString(config.automatic_proxy.source) << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PAC_SCRIPT:
      out << "Custom PAC script:\n  "
          << SourceToString(config.automatic_proxy.source)
          << "\n  PAC: " << config.automatic_proxy.pac_url << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY:
      out << "Single proxy:\n" << config.single_proxy;
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME:
      out << "HTTP proxy: " << config.http_proxy;
      out << "HTTPS proxy: " << config.https_proxy;
      out << "FTP proxy: " << config.ftp_proxy;
      out << "SOCKS proxy: " << config.socks_proxy;
      break;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
  if (config.mode == ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY ||
      config.mode ==
          ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME) {
    out << "Bypass list: ";
    if (config.bypass_rules.rules().empty()) {
      out << "[None]";
    } else {
      const net::ProxyBypassRules& bypass_rules = config.bypass_rules;
      net::ProxyBypassRules::RuleList::const_iterator it;
      for (it = bypass_rules.rules().begin();
           it != bypass_rules.rules().end(); ++it) {
        out << "\n    " << (*it)->ToString();
      }
    }
  }
  return out;
}

std::string ProxyConfigToString(
    const ProxyConfigServiceImpl::ProxyConfig& proxy_config) {
  std::ostringstream stream;
  stream << proxy_config;
  return stream.str();
}

}  // namespace

//---------- ProxyConfigServiceImpl::ProxyConfig::Setting methods --------------

bool ProxyConfigServiceImpl::ProxyConfig::Setting::CanBeWrittenByUser(
    bool user_is_owner) {
  // Setting can only be written by user if user is owner and setting is not
  // from policy.
  return user_is_owner && source != ProxyConfig::SOURCE_POLICY;
}

//----------- ProxyConfigServiceImpl::ProxyConfig: public methods --------------

ProxyConfigServiceImpl::ProxyConfig::ProxyConfig() : mode(MODE_DIRECT) {}

ProxyConfigServiceImpl::ProxyConfig::~ProxyConfig() {}

void ProxyConfigServiceImpl::ProxyConfig::ToNetProxyConfig(
    net::ProxyConfig* net_config) {
  switch (mode) {
    case MODE_DIRECT:
      *net_config = net::ProxyConfig::CreateDirect();
      break;
    case MODE_AUTO_DETECT:
      *net_config = net::ProxyConfig::CreateAutoDetect();
      break;
    case MODE_PAC_SCRIPT:
      *net_config = net::ProxyConfig::CreateFromCustomPacURL(
          automatic_proxy.pac_url);
      break;
    case MODE_SINGLE_PROXY:
      *net_config = net::ProxyConfig();
      net_config->proxy_rules().type =
             net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
      net_config->proxy_rules().single_proxy = single_proxy.server;
      net_config->proxy_rules().bypass_rules = bypass_rules;
      break;
    case MODE_PROXY_PER_SCHEME:
      *net_config = net::ProxyConfig();
      net_config->proxy_rules().type =
          net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      net_config->proxy_rules().proxy_for_http = http_proxy.server;
      net_config->proxy_rules().proxy_for_https = https_proxy.server;
      net_config->proxy_rules().proxy_for_ftp = ftp_proxy.server;
      net_config->proxy_rules().fallback_proxy = socks_proxy.server;
      net_config->proxy_rules().bypass_rules = bypass_rules;
      break;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
}

bool ProxyConfigServiceImpl::ProxyConfig::CanBeWrittenByUser(
    bool user_is_owner, const std::string& scheme) {
  // Setting can only be written by user if user is owner and setting is not
  // from policy.
  Setting* setting = NULL;
  switch (mode) {
    case MODE_DIRECT:
    case MODE_AUTO_DETECT:
    case MODE_PAC_SCRIPT:
      setting = &automatic_proxy;
      break;
    case MODE_SINGLE_PROXY:
      setting = &single_proxy;
      break;
    case MODE_PROXY_PER_SCHEME:
      setting = MapSchemeToProxy(scheme);
      break;
    default:
      break;
  }
  if (!setting) {
    NOTREACHED() << "Unrecognized proxy config mode";
    return false;
  }
  return setting->CanBeWrittenByUser(user_is_owner);
}

ProxyConfigServiceImpl::ProxyConfig::ManualProxy*
    ProxyConfigServiceImpl::ProxyConfig::MapSchemeToProxy(
        const std::string& scheme) {
  if (scheme == "http")
    return &http_proxy;
  if (scheme == "https")
    return &https_proxy;
  if (scheme == "ftp")
    return &ftp_proxy;
  if (scheme == "socks")
    return &socks_proxy;
  NOTREACHED() << "Invalid scheme: " << scheme;
  return NULL;
}

bool ProxyConfigServiceImpl::ProxyConfig::DeserializeForDevice(
    const std::string& input) {
  em::DeviceProxySettingsProto proxy_proto;
  if (!proxy_proto.ParseFromString(input))
    return false;

  const std::string& mode_string(proxy_proto.proxy_mode());
  if (mode_string == ProxyPrefs::kDirectProxyModeName) {
    mode = MODE_DIRECT;
  } else if (mode_string == ProxyPrefs::kAutoDetectProxyModeName) {
    mode = MODE_AUTO_DETECT;
  } else if (mode_string == ProxyPrefs::kPacScriptProxyModeName) {
    mode = MODE_PAC_SCRIPT;
    if (proxy_proto.has_proxy_pac_url())
      automatic_proxy.pac_url = GURL(proxy_proto.proxy_pac_url());
  } else if (mode_string == ProxyPrefs::kFixedServersProxyModeName) {
    net::ProxyConfig::ProxyRules rules;
    rules.ParseFromString(proxy_proto.proxy_server());
    switch (rules.type) {
      case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
        return false;
      case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
        if (!rules.single_proxy.is_valid())
          return false;
        mode = MODE_SINGLE_PROXY;
        single_proxy.server = rules.single_proxy;
        break;
      case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
        // Make sure we have valid server for at least one of the protocols.
        if (!rules.proxy_for_http.is_valid() &&
            !rules.proxy_for_https.is_valid() &&
            !rules.proxy_for_ftp.is_valid() &&
            !rules.fallback_proxy.is_valid()) {
          return false;
        }
        mode = MODE_PROXY_PER_SCHEME;
        if (rules.proxy_for_http.is_valid())
          http_proxy.server = rules.proxy_for_http;
        if (rules.proxy_for_https.is_valid())
          https_proxy.server = rules.proxy_for_https;
        if (rules.proxy_for_ftp.is_valid())
          ftp_proxy.server = rules.proxy_for_ftp;
        if (rules.fallback_proxy.is_valid())
          socks_proxy.server = rules.fallback_proxy;
        break;
    }
  } else {
    NOTREACHED() << "Unrecognized proxy config mode";
    return false;
  }

  if (proxy_proto.has_proxy_bypass_list())
    bypass_rules.ParseFromString(proxy_proto.proxy_bypass_list());

  return true;
}

bool ProxyConfigServiceImpl::ProxyConfig::SerializeForNetwork(
    std::string* output) {
  scoped_ptr<DictionaryValue> proxy_dict(ToPrefProxyConfig());
  if (!proxy_dict.get())
    return false;
  JSONStringValueSerializer serializer(output);
  return serializer.Serialize(*proxy_dict.get());
}

bool ProxyConfigServiceImpl::ProxyConfig::DeserializeForNetwork(
    const std::string& input) {
  JSONStringValueSerializer serializer(input);
  scoped_ptr<Value> value(serializer.Deserialize(NULL, NULL));
  if (!value.get() || value->GetType() != Value::TYPE_DICTIONARY)
    return false;
  DictionaryValue* proxy_dict = static_cast<DictionaryValue*>(value.get());
  return FromPrefProxyConfig(proxy_dict);
}

bool ProxyConfigServiceImpl::ProxyConfig::Equals(
    const ProxyConfig& other) const {
  if (mode != other.mode)
    return false;
  switch (mode) {
    case MODE_DIRECT:
    case MODE_AUTO_DETECT:
      return true;
    case MODE_PAC_SCRIPT:
      return automatic_proxy.pac_url == other.automatic_proxy.pac_url;
    case MODE_SINGLE_PROXY:
      return single_proxy.server == other.single_proxy.server &&
          bypass_rules.Equals(other.bypass_rules);
    case MODE_PROXY_PER_SCHEME:
      return http_proxy.server == other.http_proxy.server &&
          https_proxy.server == other.https_proxy.server &&
          ftp_proxy.server == other.ftp_proxy.server &&
          socks_proxy.server == other.socks_proxy.server &&
          bypass_rules.Equals(other.bypass_rules);
    default: {
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
    }
  }
  return false;
}

std::string ProxyConfigServiceImpl::ProxyConfig::ToString() const {
  return ProxyConfigToString(*this);
}

//----------- ProxyConfigServiceImpl::ProxyConfig: private methods -------------

// static
void ProxyConfigServiceImpl::ProxyConfig::EncodeAndAppendProxyServer(
    const std::string& scheme,
    const net::ProxyServer& server,
    std::string* spec) {
  if (!server.is_valid())
    return;

  if (!spec->empty())
    *spec += ';';

  if (!scheme.empty()) {
    *spec += scheme;
    *spec += "=";
  }
  *spec += server.ToURI();
}

DictionaryValue* ProxyConfigServiceImpl::ProxyConfig::ToPrefProxyConfig() {
  switch (mode) {
    case MODE_DIRECT: {
      return ProxyConfigDictionary::CreateDirect();
    }
    case MODE_AUTO_DETECT: {
      return ProxyConfigDictionary::CreateAutoDetect();
    }
    case MODE_PAC_SCRIPT: {
      return ProxyConfigDictionary::CreatePacScript(
          automatic_proxy.pac_url.spec(), false);
    }
    case MODE_SINGLE_PROXY: {
      std::string spec;
      if (single_proxy.server.is_valid())
        spec = single_proxy.server.ToURI();
      return ProxyConfigDictionary::CreateFixedServers(
           spec, bypass_rules.ToString());
    }
    case MODE_PROXY_PER_SCHEME: {
      std::string spec;
      EncodeAndAppendProxyServer("http", http_proxy.server, &spec);
      EncodeAndAppendProxyServer("https", https_proxy.server, &spec);
      EncodeAndAppendProxyServer("ftp", ftp_proxy.server, &spec);
      EncodeAndAppendProxyServer("socks", socks_proxy.server, &spec);
      return ProxyConfigDictionary::CreateFixedServers(
        spec, bypass_rules.ToString());
    }
    default:
      break;
  }
  NOTREACHED() << "Unrecognized proxy config mode for preference";
  return NULL;
}

bool ProxyConfigServiceImpl::ProxyConfig::FromPrefProxyConfig(
    const DictionaryValue* dict) {
  ProxyConfigDictionary proxy_dict(dict);

  *this = ProxyConfigServiceImpl::ProxyConfig();  // Reset to default.

  ProxyPrefs::ProxyMode proxy_mode;
  if (!proxy_dict.GetMode(&proxy_mode)) {
    // Fall back to system settings if the mode preference is invalid.
    return false;
  }

  switch (proxy_mode) {
    case ProxyPrefs::MODE_SYSTEM:
      // Use system settings, so we shouldn't use |this| proxy config.
      return false;
    case ProxyPrefs::MODE_DIRECT:
      // Ignore all the other proxy config preferences if the use of a proxy
      // has been explicitly disabled.
      return true;
    case ProxyPrefs::MODE_AUTO_DETECT:
      mode = MODE_AUTO_DETECT;
      return true;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string proxy_pac;
      if (!proxy_dict.GetPacUrl(&proxy_pac)) {
        LOG(ERROR) << "Proxy settings request PAC script but do not specify "
                   << "its URL. Falling back to direct connection.";
        return true;
      }
      GURL proxy_pac_url(proxy_pac);
      if (!proxy_pac_url.is_valid()) {
        LOG(ERROR) << "Invalid proxy PAC url: " << proxy_pac;
        return true;
      }
      mode = MODE_PAC_SCRIPT;
      automatic_proxy.pac_url = proxy_pac_url;
      return true;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      if (!proxy_dict.GetProxyServer(&proxy_server)) {
        LOG(ERROR) << "Proxy settings request fixed proxy servers but do not "
                   << "specify their URLs. Falling back to direct connection.";
        return true;
      }
      net::ProxyConfig::ProxyRules proxy_rules;
      proxy_rules.ParseFromString(proxy_server);
      if (proxy_rules.type == net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY) {
        mode = MODE_SINGLE_PROXY;
        single_proxy.server = proxy_rules.single_proxy;
      } else if (proxy_rules.type ==
                 net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME) {
        mode = MODE_PROXY_PER_SCHEME;
        http_proxy.server = proxy_rules.proxy_for_http;
        https_proxy.server = proxy_rules.proxy_for_https;
        ftp_proxy.server = proxy_rules.proxy_for_ftp;
        socks_proxy.server = proxy_rules.fallback_proxy;
      } else {
        LOG(ERROR) << "Proxy settings request fixed proxy servers but do not "
                   << "have valid proxy rules type. "
                   << "Falling back to direct connection.";
        return true;
      }

      std::string proxy_bypass;
      if (proxy_dict.GetBypassList(&proxy_bypass)) {
        bypass_rules.ParseFromString(proxy_bypass);
      }
      return true;
    }
    case ProxyPrefs::kModeCount: {
      // Fall through to NOTREACHED().
    }
  }
  NOTREACHED() << "Unknown proxy mode, falling back to system settings.";
  return false;
}

//------------------- ProxyConfigServiceImpl: public methods -------------------

ProxyConfigServiceImpl::ProxyConfigServiceImpl()
    : testing_(false),
      can_post_task_(false),
      config_availability_(net::ProxyConfigService::CONFIG_UNSET),
      use_shared_proxies_(true) {
  // Start async fetch of proxy config from settings persisted on device.
  if (CrosLibrary::Get()->EnsureLoaded()) {
    retrieve_property_op_ = SignedSettings::CreateRetrievePropertyOp(
        kSettingProxyEverywhere, this);
    if (retrieve_property_op_) {
      retrieve_property_op_->Execute();
      VLOG(1) << "Start retrieving proxy setting from device";
    } else {
      VLOG(1) << "Fail to retrieve proxy setting from device";
    }
  }

  NetworkLibrary* network_lib = CrosLibrary::Get()->GetNetworkLibrary();
  OnActiveNetworkChanged(network_lib, network_lib->active_network());
  network_lib->AddNetworkManagerObserver(this);

  can_post_task_ = true;
}

ProxyConfigServiceImpl::ProxyConfigServiceImpl(const ProxyConfig& init_config)
    : testing_(false),
      can_post_task_(true),
      config_availability_(net::ProxyConfigService::CONFIG_VALID),
      use_shared_proxies_(true) {
  active_config_ = init_config;
  // Update the IO-accessible copy in |cached_config_| as well.
  cached_config_ = active_config_;
}

ProxyConfigServiceImpl::~ProxyConfigServiceImpl() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  if (netlib) {
    netlib->RemoveNetworkManagerObserver(this);
    netlib->RemoveObserverForAllNetworks(this);
  }
}

void ProxyConfigServiceImpl::UIGetProxyConfig(ProxyConfig* config) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  // Simply returns the copy on the UI thread.
  *config = current_ui_config_;
}

bool ProxyConfigServiceImpl::UISetCurrentNetwork(
    const std::string& current_network) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  if (current_ui_network_ == current_network)
    return false;
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      current_network);
  if (!network) {
    LOG(ERROR) << "can't find requested network " << current_network;
    return false;
  }
  current_ui_network_ = current_network;
  current_ui_config_ = ProxyConfig();
  SetCurrentNetworkName(network);
  if (!network->proxy_config().empty())
    current_ui_config_.DeserializeForNetwork(network->proxy_config());
  VLOG(1) << "current ui network: "
          << (current_ui_network_name_.empty() ?
              current_ui_network_ : current_ui_network_name_)
          << ", proxy mode: " << current_ui_config_.mode;
  return true;
}

bool ProxyConfigServiceImpl::UIMakeActiveNetworkCurrent() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  if (current_ui_network_ == active_network_)
    return false;
  Network* network = NULL;
  if (!testing_) {
    network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      active_network_);
    if (!network) {
      LOG(ERROR) << "can't find requested network " << active_network_;
      return false;
    }
  }
  current_ui_network_ = active_network_;
  current_ui_config_ = active_config_;
  SetCurrentNetworkName(network);
  VLOG(1) << "current ui network: "
          << (current_ui_network_name_.empty() ?
              current_ui_network_ : current_ui_network_name_)
          << ", proxy mode: " << current_ui_config_.mode;
  return true;
}

void ProxyConfigServiceImpl::UISetUseSharedProxies(bool use_shared) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();

  // Reset all UI-related variables so that the next opening of proxy
  // configuration dialog of any network will trigger javascript reloading of
  // (possibly) new proxy settings.
  current_ui_network_.clear();
  current_ui_network_name_.clear();
  current_ui_config_ = ProxyConfig();

  if (use_shared_proxies_ == use_shared) {
    VLOG(1) << "same use_shared_proxies = " << use_shared_proxies_;
    return;
  }
  use_shared_proxies_ = use_shared;
  VLOG(1) << "new use_shared_proxies = " << use_shared_proxies_;
  if (active_network_.empty())
    return;
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      active_network_);
  if (!network) {
    LOG(ERROR) << "can't find requested network " << active_network_;
    return;
  }
  DetermineConfigFromNetwork(network);
}

bool ProxyConfigServiceImpl::UISetProxyConfigToDirect() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  current_ui_config_.mode = ProxyConfig::MODE_DIRECT;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToAutoDetect() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  current_ui_config_.mode = ProxyConfig::MODE_AUTO_DETECT;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToPACScript(const GURL& pac_url) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  current_ui_config_.mode = ProxyConfig::MODE_PAC_SCRIPT;
  current_ui_config_.automatic_proxy.pac_url = pac_url;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToSingleProxy(
    const net::ProxyServer& server) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  current_ui_config_.mode = ProxyConfig::MODE_SINGLE_PROXY;
  current_ui_config_.single_proxy.server = server;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToProxyPerScheme(
    const std::string& scheme, const net::ProxyServer& server) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  ProxyConfig::ManualProxy* proxy = current_ui_config_.MapSchemeToProxy(scheme);
  if (!proxy) {
    NOTREACHED() << "Cannot set proxy: invalid scheme [" << scheme << "]";
    return false;
  }
  current_ui_config_.mode = ProxyConfig::MODE_PROXY_PER_SCHEME;
  proxy->server = server;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigBypassRules(
    const net::ProxyBypassRules& bypass_rules) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  if (current_ui_config_.mode != ProxyConfig::MODE_SINGLE_PROXY &&
      current_ui_config_.mode != ProxyConfig::MODE_PROXY_PER_SCHEME) {
    NOTREACHED();
    VLOG(1) << "Cannot set bypass rules for proxy mode ["
             << current_ui_config_.mode << "]";
    return false;
  }
  current_ui_config_.bypass_rules = bypass_rules;
  OnUISetProxyConfig();
  return true;
}

void ProxyConfigServiceImpl::AddObserver(
    net::ProxyConfigService::Observer* observer) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();
  observers_.AddObserver(observer);
}

void ProxyConfigServiceImpl::RemoveObserver(
    net::ProxyConfigService::Observer* observer) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
    ProxyConfigServiceImpl::IOGetProxyConfig(net::ProxyConfig* net_config) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();
  if (config_availability_ == net::ProxyConfigService::CONFIG_VALID) {
    VLOG(1) << "returning proxy mode=" << cached_config_.mode;
    cached_config_.ToNetProxyConfig(net_config);
  }
  return config_availability_;
}

void ProxyConfigServiceImpl::OnSettingsOpCompleted(
    SignedSettings::ReturnCode code,
    std::string value) {
  retrieve_property_op_ = NULL;
  if (code != SignedSettings::SUCCESS) {
    LOG(WARNING) << "Error retrieving proxy setting from device";
    device_config_.clear();
    return;
  }
  VLOG(1) << "Retrieved proxy setting from device, value=[" << value << "]";
  ProxyConfig device_config;
  if (!device_config.DeserializeForDevice(value) ||
      !device_config.SerializeForNetwork(&device_config_)) {
    LOG(WARNING) << "Can't deserialize device setting or serialize for network";
    device_config_.clear();
    return;
  }
  if (!active_network_.empty()) {
    VLOG(1) << "try migrating device config to " << active_network_;
    SetProxyConfigForNetwork(active_network_, device_config_, true);
  }
}

void ProxyConfigServiceImpl::OnNetworkManagerChanged(
    NetworkLibrary* network_lib) {
  VLOG(1) << "OnNetworkManagerChanged: use-shared-proxies="
          << use_shared_proxies_;
  OnActiveNetworkChanged(network_lib, network_lib->active_network());
}

void ProxyConfigServiceImpl::OnNetworkChanged(NetworkLibrary* network_lib,
    const Network* network) {
  if (!network)
    return;
  VLOG(1) << "OnNetworkChanged: "
          << (network->name().empty() ? network->service_path() :
                                        network->name())
          << ", use-shared-proxies=" << use_shared_proxies_;
  // We only care about active network.
  if (network == network_lib->active_network())
    OnActiveNetworkChanged(network_lib, network);
}

//------------------ ProxyConfigServiceImpl: private methods -------------------

void ProxyConfigServiceImpl::OnUISetProxyConfig() {
  if (testing_) {
    active_config_ = current_ui_config_;
    IOSetProxyConfig(active_config_, net::ProxyConfigService::CONFIG_VALID);
    return;
  }
  if (current_ui_network_.empty())
    return;
  // Update config to flimflam.
  std::string value;
  if (current_ui_config_.SerializeForNetwork(&value)) {
    VLOG(1) << "set proxy (mode=" << current_ui_config_.mode
            << ") for " << current_ui_network_;
    SetProxyConfigForNetwork(current_ui_network_, value, false);
  }
}

void ProxyConfigServiceImpl::IOSetProxyConfig(
    const ProxyConfig& new_config,
    net::ProxyConfigService::ConfigAvailability new_availability) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO) && can_post_task_) {
    // Posts a task to IO thread with the new config, so it can update
    // |cached_config_|.
    Task* task = NewRunnableMethod(this,
                                   &ProxyConfigServiceImpl::IOSetProxyConfig,
                                   new_config,
                                   new_availability);
    if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task))
      VLOG(1) << "Couldn't post task to IO thread to set new proxy config";
    return;
  }

  // Now guaranteed to be on the correct thread.

  if (config_availability_ == new_availability &&
      cached_config_.Equals(new_config))
    return;

  VLOG(1) << "Proxy changed: mode=" << new_config.mode
          << ", avail=" << new_availability;
  cached_config_ = new_config;
  config_availability_ = new_availability;
  // Notify observers of new proxy config.
  net::ProxyConfig net_config;
  cached_config_.ToNetProxyConfig(&net_config);
  if (net_config.proxy_rules().type !=
      net::ProxyConfig::ProxyRules::TYPE_NO_RULES) {
    net_config.proxy_rules().bypass_rules.AddRuleToBypassLocal();
  }
  FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                    OnProxyConfigChanged(net_config, config_availability_));
}

void ProxyConfigServiceImpl::OnActiveNetworkChanged(NetworkLibrary* network_lib,
    const Network* active_network) {
  std::string new_network;
  if (active_network)
    new_network = active_network->service_path();

  if (active_network_ == new_network) {  // Same active network.
    VLOG(1) << "same active network: "
            << (new_network.empty() ? "empty" :
                (active_network->name().empty() ?
                 new_network : active_network->name()));
    return;
  }

  // If there was a previous active network, remove it as observer.
  if (!active_network_.empty())
    network_lib->RemoveNetworkObserver(active_network_, this);

  active_network_ = new_network;

  if (active_network_.empty()) {
    VLOG(1) << "new active network: empty";
    active_config_ = ProxyConfig();
    IOSetProxyConfig(active_config_, net::ProxyConfigService::CONFIG_UNSET);
    return;
  }

  VLOG(1) << "new active network: path=" << active_network->service_path()
          << ", name=" << active_network->name()
          << ", profile=" << active_network->profile_path()
          << ", proxy=" << active_network->proxy_config();

  // Register observer for new network.
  network_lib->AddNetworkObserver(active_network_, this);

  // If necessary, migrate config to flimflam.
  if (active_network->proxy_config().empty() && !device_config_.empty()) {
    VLOG(1) << "try migrating device config to " << active_network_;
    SetProxyConfigForNetwork(active_network_, device_config_, true);
  } else {
    DetermineConfigFromNetwork(active_network);
  }
}

void ProxyConfigServiceImpl::SetProxyConfigForNetwork(
    const std::string& network_path, const std::string& value,
    bool only_set_if_empty) {
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      network_path);
  if (!network) {
    NOTREACHED() << "can't find requested network " << network_path;
    return;
  }
  if (!only_set_if_empty || network->proxy_config().empty()) {
    network->SetProxyConfig(value);
    VLOG(1) << "set proxy for "
            << (network->name().empty() ? network_path : network->name())
            << ", value=" << value;
    if (network_path == active_network_)
      DetermineConfigFromNetwork(network);
  }
}

void ProxyConfigServiceImpl::DetermineConfigFromNetwork(
    const Network* network) {
  active_config_ = ProxyConfig();  // Default is DIRECT mode (i.e. no proxy).
  net::ProxyConfigService::ConfigAvailability available =
      net::ProxyConfigService::CONFIG_UNSET;
  // If network is shared but user doesn't use shared proxies, use direct mode.
  if (network->profile_type() == PROFILE_SHARED && !use_shared_proxies_) {
    VLOG(1) << "shared network and !use_shared_proxies, using direct";
    available = net::ProxyConfigService::CONFIG_VALID;
  } else if (!network->proxy_config().empty() &&
             active_config_.DeserializeForNetwork(network->proxy_config())) {
    // Network is private or shared with user using shared proxies.
    VLOG(1) << "using network proxy: " << network->proxy_config();
    available = net::ProxyConfigService::CONFIG_VALID;
  }
  IOSetProxyConfig(active_config_, available);
}

void ProxyConfigServiceImpl::SetCurrentNetworkName(const Network* network) {
  if (!network) {
    if (testing_)
      current_ui_network_name_ = "test";
    return;
  }
  if (network->name().empty() && network->type() == chromeos::TYPE_ETHERNET) {
    current_ui_network_name_ = l10n_util::GetStringUTF8(
        IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else {
    current_ui_network_name_ = network->name();
  }
}

void ProxyConfigServiceImpl::CheckCurrentlyOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ProxyConfigServiceImpl::CheckCurrentlyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

}  // namespace chromeos
