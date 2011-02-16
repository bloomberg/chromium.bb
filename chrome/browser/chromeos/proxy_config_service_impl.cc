// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <ostream>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/json_value_serializer.h"

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

// Name of signed setting persisted on device, writeable only by owner.
const char* kSettingProxyEverywhere = "cros.proxy.everywhere";

// Names used for dictionary values to serialize chromeos::ProxyConfig.
const char* kMode = "mode";
const char* kSource = "src";
const char* kAutomaticProxy = "auto";
const char* kSingleProxy = "single";
const char* kHttpProxy = "http";
const char* kHttpsProxy = "https";
const char* kFtpProxy = "ftp";
const char* kSocksProxy = "socks";
const char* kPACUrl = "pac";
const char* kServer = "server";
const char* kBypassRules = "bypass_rules";
const char* kRulesNum = "num";
const char* kRulesList = "list";

}  // namespace

//---------- ProxyConfigServiceImpl::ProxyConfig::Setting methods --------------

bool ProxyConfigServiceImpl::ProxyConfig::Setting::CanBeWrittenByUser(
    bool user_is_owner) {
  // Setting can only be written by user if user is owner and setting is not
  // from policy.
  return user_is_owner && source != ProxyConfig::SOURCE_POLICY;
}

DictionaryValue* ProxyConfigServiceImpl::ProxyConfig::Setting::Encode() const {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger(kSource, source);
  return dict;
}

bool ProxyConfigServiceImpl::ProxyConfig::Setting::Decode(
    DictionaryValue* dict) {
  int int_source;
  if (!dict->GetInteger(kSource, &int_source))
    return false;
  source = static_cast<Source>(int_source);
  return true;
}

//------- ProxyConfigServiceImpl::ProxyConfig::AutomaticProxy methods ----------

DictionaryValue*
    ProxyConfigServiceImpl::ProxyConfig::AutomaticProxy::Encode() const {
  DictionaryValue* dict = Setting::Encode();
  if (!pac_url.is_empty())
    dict->SetString(kPACUrl, pac_url.spec());
  return dict;
}

bool ProxyConfigServiceImpl::ProxyConfig::AutomaticProxy::Decode(
    DictionaryValue* dict, Mode mode) {
  if (!Setting::Decode(dict))
    return false;
  if (mode == MODE_PAC_SCRIPT) {
    std::string value;
    if (!dict->GetString(kPACUrl, &value))
      return false;
    pac_url = GURL(value);
  }
  return true;
}

//--------- ProxyConfigServiceImpl::ProxyConfig::ManualProxy methods -----------

DictionaryValue*
    ProxyConfigServiceImpl::ProxyConfig::ManualProxy::Encode() const {
  DictionaryValue* dict = Setting::Encode();
  dict->SetString(kServer, server.ToURI());
  return dict;
}

bool ProxyConfigServiceImpl::ProxyConfig::ManualProxy::Decode(
    DictionaryValue* dict, net::ProxyServer::Scheme scheme) {
  if (!Setting::Decode(dict))
    return false;
  std::string value;
  if (!dict->GetString(kServer, &value))
    return false;
  server = net::ProxyServer::FromURI(value, scheme);
  return true;
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
  scoped_ptr<DictionaryValue> dict(new DictionaryValue);
  dict->SetInteger(kMode, mode);
  DictionaryValue* proxy_dict;
  switch (mode) {
    case MODE_DIRECT:
    case MODE_AUTO_DETECT:
    case MODE_PAC_SCRIPT:
      proxy_dict = automatic_proxy.Encode();
      dict->Set(kAutomaticProxy, proxy_dict);
      break;
    case MODE_SINGLE_PROXY:
      EncodeManualProxy(single_proxy, dict.get(), kSingleProxy);
      break;
    case MODE_PROXY_PER_SCHEME:
      EncodeManualProxy(http_proxy, dict.get(), kHttpProxy);
      EncodeManualProxy(https_proxy, dict.get(), kHttpsProxy);
      EncodeManualProxy(ftp_proxy, dict.get(), kFtpProxy);
      EncodeManualProxy(socks_proxy, dict.get(), kSocksProxy);
      break;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
  net::ProxyBypassRules::RuleList rules = bypass_rules.rules();
  if (!rules.empty()) {
    DictionaryValue* bypass_dict = new DictionaryValue;
    bypass_dict->SetInteger(kRulesNum, rules.size());
    ListValue* list = new ListValue;
    for (size_t i = 0; i < rules.size(); ++i) {
      list->Append(Value::CreateStringValue(rules[i]->ToString()));
    }
    bypass_dict->Set(kRulesList, list);
    dict->Set(kBypassRules, bypass_dict);
  }
  JSONStringValueSerializer serializer(output);
  return serializer.Serialize(*dict.get());
}

bool ProxyConfigServiceImpl::ProxyConfig::Deserialize(
    const std::string& input) {
  JSONStringValueSerializer serializer(input);
  scoped_ptr<Value> value(serializer.Deserialize(NULL, NULL));
  if (!value.get() || value->GetType() != Value::TYPE_DICTIONARY)
    return false;
  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  int int_mode;
  if (!dict->GetInteger(kMode, &int_mode))
    return false;
  mode = static_cast<Mode>(int_mode);
  DictionaryValue* proxy_dict = NULL;
  switch (mode) {
    case MODE_DIRECT:
    case MODE_AUTO_DETECT:
    case MODE_PAC_SCRIPT:
      if (!dict->GetDictionary(kAutomaticProxy, &proxy_dict) ||
          !automatic_proxy.Decode(proxy_dict, mode))
        return false;
      break;
    case MODE_SINGLE_PROXY:
      if (!DecodeManualProxy(dict, kSingleProxy, false,
                             net::ProxyServer::SCHEME_HTTP, &single_proxy))
        return false;
      break;
    case MODE_PROXY_PER_SCHEME:
      if (!DecodeManualProxy(dict, kHttpProxy, true,
                             net::ProxyServer::SCHEME_HTTP, &http_proxy))
        return false;
      if (!DecodeManualProxy(dict, kHttpsProxy, true,
                             net::ProxyServer::SCHEME_HTTP, &https_proxy))
        return false;
      if (!DecodeManualProxy(dict, kFtpProxy, true,
                             net::ProxyServer::SCHEME_HTTP, &ftp_proxy))
        return false;
      if (!DecodeManualProxy(dict, kSocksProxy, true,
                             net::ProxyServer::SCHEME_SOCKS5, &socks_proxy))
        return false;
      // Make sure we have valid server for at least one of the protocols.
      if (!(http_proxy.server.is_valid() || https_proxy.server.is_valid() ||
            ftp_proxy.server.is_valid() || socks_proxy.server.is_valid()))
        return false;
      break;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
  DictionaryValue* bypass_dict = NULL;
  if (dict->GetDictionary(kBypassRules, &bypass_dict)) {
    int num_rules = 0;
    if (bypass_dict->GetInteger(kRulesNum, &num_rules) && num_rules > 0) {
      ListValue* list;
      if (!bypass_dict->GetList(kRulesList, &list))
        return false;
      for (size_t i = 0; i < list->GetSize(); ++i) {
        std::string rule;
        if (!list->GetString(i, &rule))
          return false;
        bypass_rules.AddRuleFromString(rule);
      }
    }
  }
  return true;
}

std::string ProxyConfigServiceImpl::ProxyConfig::ToString() const {
  return ProxyConfigToString(*this);
}

//----------- ProxyConfigServiceImpl::ProxyConfig: private methods -------------

void ProxyConfigServiceImpl::ProxyConfig::EncodeManualProxy(
    const ManualProxy& manual_proxy, DictionaryValue* dict,
    const char* key_name) {
  if (!manual_proxy.server.is_valid())
    return;
  DictionaryValue* proxy_dict = manual_proxy.Encode();
  dict->Set(key_name, proxy_dict);
}

bool ProxyConfigServiceImpl::ProxyConfig::DecodeManualProxy(
    DictionaryValue* dict, const char* key_name, bool ok_if_absent,
    net::ProxyServer::Scheme scheme, ManualProxy* manual_proxy) {
  DictionaryValue* proxy_dict;
  if (!dict->GetDictionary(key_name, &proxy_dict))
    return ok_if_absent;
  return manual_proxy->Decode(proxy_dict, scheme);
}

//------------------- ProxyConfigServiceImpl: public methods -------------------

ProxyConfigServiceImpl::ProxyConfigServiceImpl()
    : can_post_task_(false),
      has_config_(false),
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
    InitConfigToDefault(false);
  can_post_task_ = true;
}

ProxyConfigServiceImpl::ProxyConfigServiceImpl(const ProxyConfig& init_config)
    : can_post_task_(true),
      has_config_(true),
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

bool ProxyConfigServiceImpl::IOGetProxyConfig(net::ProxyConfig* net_config) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();
  if (has_config_) {
    // Simply return the last cached proxy configuration.
    cached_config_.ToNetProxyConfig(net_config);
    return true;
  }
  return false;
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
  if (SignedSettings::SUCCESS == code) {
    VLOG(1) << "Retrieved proxy setting from device, value=[" << value << "]";
    if (reference_config_.Deserialize(value)) {
      OnUISetProxyConfig(false);
    } else {
      LOG(WARNING) << "Error deserializing device's proxy setting";
      InitConfigToDefault(true);
    }
  } else {
    LOG(WARNING) << "Error retrieving proxy setting from device";
    InitConfigToDefault(true);
  }
  retrieve_property_op_ = NULL;
}

//------------------ ProxyConfigServiceImpl: private methods -------------------

void ProxyConfigServiceImpl::InitConfigToDefault(bool post_to_io_thread) {
  VLOG(1) << "Using default proxy config: auto-detect";
  reference_config_.mode = ProxyConfig::MODE_AUTO_DETECT;
  reference_config_.automatic_proxy.source = ProxyConfig::SOURCE_OWNER;
  if (post_to_io_thread && can_post_task_) {
    OnUISetProxyConfig(false);
  } else {
    // Update the IO-accessible copy in |cached_config_| as well.
    cached_config_ = reference_config_;
    has_config_ = true;
  }
}

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
  // Posts a task to IO thread with the new config, so it can update
  // |cached_config_|.
  Task* task = NewRunnableMethod(this,
      &ProxyConfigServiceImpl::IOSetProxyConfig, reference_config_);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    VLOG(1) << "Couldn't post task to IO thread to set new proxy config";
    delete task;
  }

  if (persist_to_device && CrosLibrary::Get()->EnsureLoaded()) {
    if (store_property_op_) {
      persist_to_device_pending_ = true;
      VLOG(1) << "Pending persisting proxy setting to device";
    } else {
      PersistConfigToDevice();
    }
  }
}

void ProxyConfigServiceImpl::CheckCurrentlyOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ProxyConfigServiceImpl::CheckCurrentlyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ProxyConfigServiceImpl::IOSetProxyConfig(const ProxyConfig& new_config) {
  // This is called on the IO thread (posted from UI thread).
  CheckCurrentlyOnIOThread();
  VLOG(1) << "Proxy configuration changed";
  has_config_ = true;
  cached_config_ = new_config;
  // Notify observers of new proxy config.
  net::ProxyConfig net_config;
  cached_config_.ToNetProxyConfig(&net_config);
  FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                    OnProxyConfigChanged(net_config));
}

}  // namespace chromeos
