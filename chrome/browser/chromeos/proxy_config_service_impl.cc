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
#include "chrome/browser/prefs/proxy_prefs.h"
#include "content/browser/browser_thread.h"

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

bool ProxyConfigServiceImpl::ProxyConfig::Serialize(std::string* output) {
  em::DeviceProxySettingsProto proxy_proto;
  switch (mode) {
    case MODE_DIRECT: {
      proxy_proto.set_proxy_mode(ProxyPrefs::kDirectProxyModeName);
      break;
    }
    case MODE_AUTO_DETECT: {
      proxy_proto.set_proxy_mode(ProxyPrefs::kAutoDetectProxyModeName);
      break;
    }
    case MODE_PAC_SCRIPT: {
      proxy_proto.set_proxy_mode(ProxyPrefs::kPacScriptProxyModeName);
      if (!automatic_proxy.pac_url.is_empty())
        proxy_proto.set_proxy_pac_url(automatic_proxy.pac_url.spec());
      break;
    }
    case MODE_SINGLE_PROXY: {
      proxy_proto.set_proxy_mode(ProxyPrefs::kFixedServersProxyModeName);
      if (single_proxy.server.is_valid())
        proxy_proto.set_proxy_server(single_proxy.server.ToURI());
      break;
    }
    case MODE_PROXY_PER_SCHEME: {
      proxy_proto.set_proxy_mode(ProxyPrefs::kFixedServersProxyModeName);
      std::string spec;
      EncodeAndAppendProxyServer("http", http_proxy.server, &spec);
      EncodeAndAppendProxyServer("https", https_proxy.server, &spec);
      EncodeAndAppendProxyServer("ftp", ftp_proxy.server, &spec);
      EncodeAndAppendProxyServer("socks", socks_proxy.server, &spec);
      if (!spec.empty())
        proxy_proto.set_proxy_server(spec);
      break;
    }
    default: {
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
    }
  }
  proxy_proto.set_proxy_bypass_list(bypass_rules.ToString());
  return proxy_proto.SerializeToString(output);
}

bool ProxyConfigServiceImpl::ProxyConfig::Deserialize(
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

//------------------- ProxyConfigServiceImpl: public methods -------------------

ProxyConfigServiceImpl::ProxyConfigServiceImpl()
    : can_post_task_(false),
      config_availability_(net::ProxyConfigService::CONFIG_PENDING),
      persist_to_device_(true),
      persist_to_device_pending_(false) {
  // Start async fetch of proxy config from settings persisted on device.
  // TODO(kuan): retrieve config from policy and owner and merge them
  bool use_default = true;
  if (CrosLibrary::Get()->EnsureLoaded()) {
    retrieve_property_op_ = SignedSettings::CreateRetrievePropertyOp(
        kSettingProxyEverywhere, this);
    if (retrieve_property_op_) {
      retrieve_property_op_->Execute();
      VLOG(1) << "Start retrieving proxy setting from device";
      use_default = false;
    } else {
      VLOG(1) << "Fail to retrieve proxy setting from device";
    }
  }
  if (use_default)
    config_availability_ = net::ProxyConfigService::CONFIG_UNSET;
  can_post_task_ = true;
}

ProxyConfigServiceImpl::ProxyConfigServiceImpl(const ProxyConfig& init_config)
    : can_post_task_(true),
      config_availability_(net::ProxyConfigService::CONFIG_VALID),
      persist_to_device_(false),
      persist_to_device_pending_(false) {
  reference_config_ = init_config;
  // Update the IO-accessible copy in |cached_config_| as well.
  cached_config_ = reference_config_;
}

ProxyConfigServiceImpl::~ProxyConfigServiceImpl() {
}

void ProxyConfigServiceImpl::UIGetProxyConfig(ProxyConfig* config) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  // Simply returns the copy on the UI thread.
  *config = reference_config_;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToDirect() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_DIRECT;
  OnUISetProxyConfig(persist_to_device_);
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToAutoDetect() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_AUTO_DETECT;
  OnUISetProxyConfig(persist_to_device_);
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToPACScript(const GURL& pac_url) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_PAC_SCRIPT;
  reference_config_.automatic_proxy.pac_url = pac_url;
  OnUISetProxyConfig(persist_to_device_);
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToSingleProxy(
    const net::ProxyServer& server) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_SINGLE_PROXY;
  reference_config_.single_proxy.server = server;
  OnUISetProxyConfig(persist_to_device_);
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToProxyPerScheme(
    const std::string& scheme, const net::ProxyServer& server) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  ProxyConfig::ManualProxy* proxy = reference_config_.MapSchemeToProxy(scheme);
  if (!proxy) {
    NOTREACHED() << "Cannot set proxy: invalid scheme [" << scheme << "]";
    return false;
  }
  reference_config_.mode = ProxyConfig::MODE_PROXY_PER_SCHEME;
  proxy->server = server;
  OnUISetProxyConfig(persist_to_device_);
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigBypassRules(
    const net::ProxyBypassRules& bypass_rules) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  DCHECK(reference_config_.mode == ProxyConfig::MODE_SINGLE_PROXY ||
         reference_config_.mode == ProxyConfig::MODE_PROXY_PER_SCHEME);
  if (reference_config_.mode != ProxyConfig::MODE_SINGLE_PROXY &&
      reference_config_.mode != ProxyConfig::MODE_PROXY_PER_SCHEME) {
    VLOG(1) << "Cannot set bypass rules for proxy mode ["
             << reference_config_.mode << "]";
    return false;
  }
  reference_config_.bypass_rules = bypass_rules;
  OnUISetProxyConfig(persist_to_device_);
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
  if (config_availability_ == net::ProxyConfigService::CONFIG_VALID)
    cached_config_.ToNetProxyConfig(net_config);

  return config_availability_;
}

void ProxyConfigServiceImpl::OnSettingsOpCompleted(
    SignedSettings::ReturnCode code,
    bool value) {
  if (SignedSettings::SUCCESS == code)
    VLOG(1) << "Stored proxy setting to device";
  else
    LOG(WARNING) << "Error storing proxy setting to device";
  store_property_op_ = NULL;
  if (persist_to_device_pending_)
    PersistConfigToDevice();
}

void ProxyConfigServiceImpl::OnSettingsOpCompleted(
    SignedSettings::ReturnCode code,
    std::string value) {
  retrieve_property_op_ = NULL;
  if (SignedSettings::SUCCESS == code) {
    VLOG(1) << "Retrieved proxy setting from device, value=[" << value << "]";
    if (reference_config_.Deserialize(value)) {
      IOSetProxyConfig(reference_config_,
                       net::ProxyConfigService::CONFIG_VALID);
      return;
    } else {
      LOG(WARNING) << "Error deserializing device's proxy setting";
    }
  } else {
    LOG(WARNING) << "Error retrieving proxy setting from device";
  }

  // Update the configuration state on the IO thread.
  IOSetProxyConfig(reference_config_, net::ProxyConfigService::CONFIG_UNSET);
}

//------------------ ProxyConfigServiceImpl: private methods -------------------

void ProxyConfigServiceImpl::PersistConfigToDevice() {
  DCHECK(!store_property_op_);
  persist_to_device_pending_ = false;
  std::string value;
  if (!reference_config_.Serialize(&value)) {
    LOG(WARNING) << "Error serializing proxy config";
    return;
  }
  store_property_op_ = SignedSettings::CreateStorePropertyOp(
      kSettingProxyEverywhere, value, this);
  store_property_op_->Execute();
  VLOG(1) << "Start storing proxy setting to device, value=" << value;
}

void ProxyConfigServiceImpl::OnUISetProxyConfig(bool persist_to_device) {
  IOSetProxyConfig(reference_config_, net::ProxyConfigService::CONFIG_VALID);
  if (persist_to_device && CrosLibrary::Get()->EnsureLoaded()) {
    if (store_property_op_) {
      persist_to_device_pending_ = true;
      VLOG(1) << "Pending persisting proxy setting to device";
    } else {
      PersistConfigToDevice();
    }
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
  VLOG(1) << "Proxy configuration changed";
  cached_config_ = new_config;
  config_availability_ = new_availability;
  // Notify observers of new proxy config.
  net::ProxyConfig net_config;
  cached_config_.ToNetProxyConfig(&net_config);
  FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                    OnProxyConfigChanged(net_config, config_availability_));
}

void ProxyConfigServiceImpl::CheckCurrentlyOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ProxyConfigServiceImpl::CheckCurrentlyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

}  // namespace chromeos
