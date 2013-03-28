// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <ostream>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/onc/onc_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

// Shoud we try to push this to base?
// Helper comparator functor for the find_if call in |findIfEqual|
template <class T>
class EqualsComparator{
 public:
  explicit EqualsComparator(const T& key) : key_(key) { }
  bool operator() (const T& element) {
    return element.Equals(key_);
  }
 private:
  const T& key_;
};

// Tiny STL helper function to allow using the find_if syntax on objects that
// doesn't use the operator== but implement the Equals function which is the
// quasi standard with the coding style we have.
template<class InputIterator, class T>
InputIterator findIfEqual(InputIterator first, InputIterator last,
                          const T& key) {
  return std::find_if(first, last, EqualsComparator<T>(key));
}

const char* ModeToString(ProxyConfigServiceImpl::ProxyConfig::Mode mode) {
  switch (mode) {
    case ProxyConfigServiceImpl::ProxyConfig::MODE_DIRECT:
      return "direct";
    case ProxyConfigServiceImpl::ProxyConfig::MODE_AUTO_DETECT:
      return "auto-detect";
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PAC_SCRIPT:
      return "pacurl";
    case ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY:
      return "single-proxy";
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME:
      return "proxy-per-scheme";
  }
  NOTREACHED() << "Unrecognized mode type";
  return "";
}

const char* ConfigStateToString(ProxyPrefs::ConfigState state) {
  switch (state) {
    case ProxyPrefs::CONFIG_POLICY:
      return "config_policy";
    case ProxyPrefs::CONFIG_EXTENSION:
      return "config_extension";
    case ProxyPrefs::CONFIG_OTHER_PRECEDE:
      return "config_other_precede";
    case ProxyPrefs::CONFIG_SYSTEM:
      return "config_network";  // For ChromeOS, system is network.
    case ProxyPrefs::CONFIG_FALLBACK:
      return "config_recommended";  // Fallback is recommended.
    case ProxyPrefs::CONFIG_UNSET:
      return "config_unset";
  }
  NOTREACHED() << "Unrecognized config state type";
  return "";
}

// Returns true if proxy settings from |network| is editable.
bool IsNetworkProxySettingsEditable(const Network* network) {
  if (!network)
    return true;  // editable if no network given.

  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  const base::DictionaryValue* onc =
       network_library->FindOncForNetwork(network->unique_id());

  NetworkPropertyUIData proxy_settings_ui_data;
  proxy_settings_ui_data.ParseOncProperty(
      network->ui_data(),
      onc,
      onc::network_config::kProxySettings);
  return proxy_settings_ui_data.editable();
}

// Only unblock if needed for debugging.
#if defined(NEED_DEBUG_LOG)
std::ostream& operator<<(
    std::ostream& out,
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) {
  out << (proxy.server.is_valid() ? proxy.server.ToURI() : "") << "\n";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const ProxyConfigServiceImpl::ProxyConfig& config) {
  switch (config.mode) {
    case ProxyConfigServiceImpl::ProxyConfig::MODE_DIRECT:
    case ProxyConfigServiceImpl::ProxyConfig::MODE_AUTO_DETECT:
      out << ModeToString(config.mode) << ", "
          << ConfigStateToString(config.state) << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PAC_SCRIPT:
      out << ModeToString(config.mode) << ", "
          << ConfigStateToString(config.state)
          << "\n  PAC: " << config.automatic_proxy.pac_url << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY:
      out << ModeToString(config.mode) << ", "
          << ConfigStateToString(config.state) << "\n  " << config.single_proxy;
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME:
      out << ModeToString(config.mode) << ", "
          << ConfigStateToString(config.state) << "\n"
          << "  HTTP:  " << config.http_proxy
          << "  HTTPS: " << config.https_proxy
          << "  FTP: " << config.ftp_proxy
          << "  SOCKS: " << config.socks_proxy;
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
#endif  // defined(NEED_DEBUG_LOG)

}  // namespace

//----------- ProxyConfigServiceImpl::ProxyConfig: public methods --------------

ProxyConfigServiceImpl::ProxyConfig::ProxyConfig()
    : mode(MODE_DIRECT),
      state(ProxyPrefs::CONFIG_UNSET),
      user_modifiable(true) {}

ProxyConfigServiceImpl::ProxyConfig::~ProxyConfig() {}

bool ProxyConfigServiceImpl::ProxyConfig::FromNetProxyConfig(
    const net::ProxyConfig& net_config) {
  *this = ProxyConfigServiceImpl::ProxyConfig();  // Reset to default.
  const net::ProxyConfig::ProxyRules& rules = net_config.proxy_rules();
  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      if (!net_config.HasAutomaticSettings()) {
        mode = ProxyConfig::MODE_DIRECT;
      } else if (net_config.auto_detect()) {
        mode = ProxyConfig::MODE_AUTO_DETECT;
      } else if (net_config.has_pac_url()) {
        mode = ProxyConfig::MODE_PAC_SCRIPT;
        automatic_proxy.pac_url = net_config.pac_url();
      } else {
        return false;
      }
      return true;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      if (rules.single_proxies.IsEmpty())
        return false;
      mode = MODE_SINGLE_PROXY;
      single_proxy.server = rules.single_proxies.Get();
      bypass_rules = rules.bypass_rules;
      return true;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      // Make sure we have valid server for at least one of the protocols.
      if (rules.proxies_for_http.IsEmpty() &&
          rules.proxies_for_https.IsEmpty() &&
          rules.proxies_for_ftp.IsEmpty() &&
          rules.fallback_proxies.IsEmpty()) {
        return false;
      }
      mode = MODE_PROXY_PER_SCHEME;
      if (!rules.proxies_for_http.IsEmpty())
        http_proxy.server = rules.proxies_for_http.Get();
      if (!rules.proxies_for_https.IsEmpty())
        https_proxy.server = rules.proxies_for_https.Get();
      if (!rules.proxies_for_ftp.IsEmpty())
        ftp_proxy.server = rules.proxies_for_ftp.Get();
      if (!rules.fallback_proxies.IsEmpty())
        socks_proxy.server = rules.fallback_proxies.Get();
      bypass_rules = rules.bypass_rules;
      return true;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
  return false;
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
        if (rules.single_proxies.IsEmpty())
          return false;
        mode = MODE_SINGLE_PROXY;
        single_proxy.server = rules.single_proxies.Get();
        return true;
      case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
        // Make sure we have valid server for at least one of the protocols.
        if (rules.proxies_for_http.IsEmpty() &&
            rules.proxies_for_https.IsEmpty() &&
            rules.proxies_for_ftp.IsEmpty() &&
            rules.fallback_proxies.IsEmpty()) {
          return false;
        }
        mode = MODE_PROXY_PER_SCHEME;
        if (!rules.proxies_for_http.IsEmpty())
          http_proxy.server = rules.proxies_for_http.Get();
        if (!rules.proxies_for_https.IsEmpty())
          https_proxy.server = rules.proxies_for_https.Get();
        if (!rules.proxies_for_ftp.IsEmpty())
          ftp_proxy.server = rules.proxies_for_ftp.Get();
        if (!rules.fallback_proxies.IsEmpty())
          socks_proxy.server = rules.fallback_proxies.Get();
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
  scoped_ptr<DictionaryValue> proxy_dict_ptr(ToPrefProxyConfig());
  if (!proxy_dict_ptr.get())
    return false;

  // Return empty string for direct mode for portal check to work correctly.
  DictionaryValue *dict = proxy_dict_ptr.get();
  ProxyConfigDictionary proxy_dict(dict);
  ProxyPrefs::ProxyMode mode;
  if (proxy_dict.GetMode(&mode)) {
    if (mode == ProxyPrefs::MODE_DIRECT) {
      output->clear();
      return true;
    }
  }
  JSONStringValueSerializer serializer(output);
  return serializer.Serialize(*dict);
}

//----------- ProxyConfigServiceImpl::ProxyConfig: private methods -------------

// static
void ProxyConfigServiceImpl::ProxyConfig::EncodeAndAppendProxyServer(
    const std::string& url_scheme,
    const net::ProxyServer& server,
    std::string* spec) {
  if (!server.is_valid())
    return;

  if (!spec->empty())
    *spec += ';';

  if (!url_scheme.empty()) {
    *spec += url_scheme;
    *spec += "=";
  }
  *spec += server.ToURI();
}

//------------------- ProxyConfigServiceImpl: public methods -------------------

ProxyConfigServiceImpl::ProxyConfigServiceImpl(PrefService* pref_service)
    : PrefProxyConfigTrackerImpl(pref_service),
      active_config_state_(ProxyPrefs::CONFIG_UNSET),
      pointer_factory_(this) {

  // Register for notifications of UseSharedProxies user preference.
  if (pref_service->FindPreference(prefs::kUseSharedProxies)) {
    use_shared_proxies_.Init(
        prefs::kUseSharedProxies, pref_service,
        base::Bind(&ProxyConfigServiceImpl::OnUseSharedProxiesChanged,
                   base::Unretained(this)));
  }

  FetchProxyPolicy();

  // Register for shill network notifications.
  NetworkLibrary* network_lib = CrosLibrary::Get()->GetNetworkLibrary();
  OnActiveNetworkChanged(network_lib, network_lib->active_network());
  network_lib->AddNetworkManagerObserver(this);
}

ProxyConfigServiceImpl::~ProxyConfigServiceImpl() {
  NetworkLibrary* netlib = CrosLibrary::Get()->GetNetworkLibrary();
  if (netlib) {
    netlib->RemoveNetworkManagerObserver(this);
    netlib->RemoveObserverForAllNetworks(this);
  }
}

void ProxyConfigServiceImpl::UISetCurrentNetwork(
    const std::string& current_network) {
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      current_network);
  if (!network) {
    ResetUICache();
    LOG(ERROR) << "Can't find requested network " << current_network;
    return;
  }
  current_ui_network_ = current_network;
  OnUISetCurrentNetwork(network);
}

void ProxyConfigServiceImpl::UIMakeActiveNetworkCurrent() {
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      active_network_);
  if (!network) {
    ResetUICache();
    LOG(ERROR) << "Can't find requested network " << active_network_;
    return;
  }
  current_ui_network_ = active_network_;
  OnUISetCurrentNetwork(network);
}

void ProxyConfigServiceImpl::UIGetCurrentNetworkName(
    std::string* network_name) {
  if (!network_name)
    return;
  network_name->clear();
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      current_ui_network_);
  if (!network) {
    LOG(ERROR) << "Can't find requested network " << current_ui_network_;
    return;
  }
  if (network->name().empty() && network->type() == chromeos::TYPE_ETHERNET) {
    *network_name =
        l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else {
    *network_name = network->name();
  }
}

void ProxyConfigServiceImpl::UIGetProxyConfig(ProxyConfig* config) {
  // Simply returns the copy last set from UI via UISetCurrentNetwork or
  // UIMakeActiveNetworkCurrent.
  *config = current_ui_config_;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToDirect() {
  current_ui_config_.mode = ProxyConfig::MODE_DIRECT;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToAutoDetect() {
  current_ui_config_.mode = ProxyConfig::MODE_AUTO_DETECT;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToPACScript(const GURL& pac_url) {
  current_ui_config_.mode = ProxyConfig::MODE_PAC_SCRIPT;
  current_ui_config_.automatic_proxy.pac_url = pac_url;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToSingleProxy(
    const net::ProxyServer& server) {
  current_ui_config_.mode = ProxyConfig::MODE_SINGLE_PROXY;
  current_ui_config_.single_proxy.server = server;
  OnUISetProxyConfig();
  return true;
}

bool ProxyConfigServiceImpl::UISetProxyConfigToProxyPerScheme(
    const std::string& scheme, const net::ProxyServer& server) {
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

void ProxyConfigServiceImpl::AddNotificationCallback(base::Closure callback) {

  std::vector<base::Closure>::iterator iter =
      findIfEqual(callbacks_.begin(), callbacks_.end(), callback);
  if (iter == callbacks_.end())
    callbacks_.push_back(callback);
}

void ProxyConfigServiceImpl::RemoveNotificationCallback(
    base::Closure callback) {
  std::vector<base::Closure>::iterator iter =
      findIfEqual(callbacks_.begin(), callbacks_.end(), callback);
  if (iter != callbacks_.end())
    callbacks_.erase(iter);
}

void ProxyConfigServiceImpl::OnProxyConfigChanged(
    ProxyPrefs::ConfigState config_state,
    const net::ProxyConfig& config) {
  VLOG(1) << "Got prefs change: " << ConfigStateToString(config_state)
          << ", mode=" << config.proxy_rules().type;
  Network* network = NULL;
  if (!active_network_.empty()) {
    network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
        active_network_);
    if (!network)
      LOG(ERROR) << "can't find requested network " << active_network_;
  }
  DetermineEffectiveConfig(network, true);
}

void ProxyConfigServiceImpl::OnNetworkManagerChanged(
    NetworkLibrary* network_lib) {
  VLOG(1) << "OnNetworkManagerChanged: use-shared-proxies="
          << GetUseSharedProxies();
  OnActiveNetworkChanged(network_lib, network_lib->active_network());
}

void ProxyConfigServiceImpl::OnNetworkChanged(NetworkLibrary* network_lib,
    const Network* network) {
  if (!network)
    return;
  VLOG(1) << "OnNetworkChanged: "
          << (network->name().empty() ? network->service_path() :
                                        network->name())
          << ", use-shared-proxies=" << GetUseSharedProxies();
  // We only care about active network.
  if (network == network_lib->active_network())
    OnActiveNetworkChanged(network_lib, network);
}

// static
bool ProxyConfigServiceImpl::ParseProxyConfig(const Network* network,
                                              net::ProxyConfig* proxy_config) {
  if (!network || !proxy_config)
    return false;
  JSONStringValueSerializer serializer(network->proxy_config());
  scoped_ptr<Value> value(serializer.Deserialize(NULL, NULL));
  if (!value.get() || value->GetType() != Value::TYPE_DICTIONARY)
    return false;
  ProxyConfigDictionary proxy_dict(static_cast<DictionaryValue*>(value.get()));
  return PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(proxy_dict,
                                                           proxy_config);
}

// static
void ProxyConfigServiceImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  // Use shared proxies default to off.  GetUseSharedProxies will return the
  // correct value based on pre-login and login.
  registry->RegisterBooleanPref(prefs::kUseSharedProxies, true);
}

// static
void ProxyConfigServiceImpl::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUseSharedProxies,
                                true,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}

//------------------ ProxyConfigServiceImpl: private methods -------------------

void ProxyConfigServiceImpl::OnUseSharedProxiesChanged() {
  VLOG(1) << "New use-shared-proxies = " << GetUseSharedProxies();

  // Determine new proxy config which may have changed because of new
  // use-shared-proxies. If necessary, activate it.
  Network* network = NULL;
  if (!active_network_.empty()) {
    network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
        active_network_);
    if (!network)
      LOG(WARNING) << "Can't find requested network " << active_network_;
  }
  DetermineEffectiveConfig(network, true);
}

void ProxyConfigServiceImpl::OnUISetProxyConfig() {
  if (current_ui_network_.empty())
    return;
  // Update config to shill.
  std::string value;
  if (current_ui_config_.SerializeForNetwork(&value)) {
    VLOG(1) << "Set proxy (mode=" << current_ui_config_.mode
            << ") for " << current_ui_network_;
    current_ui_config_.state = ProxyPrefs::CONFIG_SYSTEM;
    SetProxyConfigForNetwork(current_ui_network_, value, false);
  }
}

void ProxyConfigServiceImpl::OnActiveNetworkChanged(NetworkLibrary* network_lib,
    const Network* active_network) {
  std::string new_network;
  if (active_network)
    new_network = active_network->service_path();

  if (active_network_ == new_network) {  // Same active network.
    VLOG(1) << "Same active network: "
            << (new_network.empty() ? "empty" :
                    (active_network->name().empty() ?
                         new_network : active_network->name()));
    // Even though network is the same, its proxy config (e.g. if private
    // version of network replaces the shared version after login), or
    // use-shared-proxies setting (e.g. after login) may have changed,
    // so re-determine effective proxy config, and activate if different.
    if (active_network) {
      VLOG(1) << "Profile=" << active_network->profile_type()
              << "," << active_network->profile_path()
              << ", proxy=" << active_network->proxy_config();
      DetermineEffectiveConfig(active_network, true);
    }
    return;
  }

  // If there was a previous active network, remove it as observer.
  if (!active_network_.empty())
    network_lib->RemoveNetworkObserver(active_network_, this);

  active_network_ = new_network;

  if (active_network_.empty()) {
    VLOG(1) << "New active network: empty";
    DetermineEffectiveConfig(active_network, true);
    return;
  }

  VLOG(1) << "New active network: path=" << active_network->service_path()
          << ", name=" << active_network->name()
          << ", profile=" << active_network->profile_type()
          << "," << active_network->profile_path()
          << ", proxy=" << active_network->proxy_config();

  // Register observer for new network.
  network_lib->AddNetworkObserver(active_network_, this);

  // If necessary, migrate config to shill.
  if (active_network->proxy_config().empty() && !device_config_.empty()) {
    VLOG(1) << "Try migrating device config to " << active_network_;
    SetProxyConfigForNetwork(active_network_, device_config_, true);
  } else  {
    // Otherwise, determine and activate possibly new effective proxy config.
    DetermineEffectiveConfig(active_network, true);
  }
}

void ProxyConfigServiceImpl::SetProxyConfigForNetwork(
    const std::string& network_path, const std::string& value,
    bool only_set_if_empty) {
  Network* network = CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(
      network_path);
  if (!network) {
    NOTREACHED() << "Can't find requested network " << network_path;
    return;
  }
  if (!only_set_if_empty || network->proxy_config().empty()) {
    network->SetProxyConfig(value);
    VLOG(1) << "Set proxy for "
            << (network->name().empty() ? network_path : network->name())
            << ", value=" << value;
    if (network_path == active_network_)
      DetermineEffectiveConfig(network, true);
  }
}

bool ProxyConfigServiceImpl::GetUseSharedProxies() {
  const PrefService::Preference* use_shared_proxies_pref =
      prefs()->FindPreference(prefs::kUseSharedProxies);
  if (!use_shared_proxies_pref) {
    // Make sure that proxies are always enabled at sign in screen.
    return !UserManager::Get()->IsUserLoggedIn();
  }
  return use_shared_proxies_.GetValue();
}

bool ProxyConfigServiceImpl::IgnoreProxy(const Network* network) {
  if (network->profile_type() == PROFILE_USER)
    return false;

  if (network->ui_data().onc_source() == onc::ONC_SOURCE_DEVICE_POLICY &&
      UserManager::Get()->IsUserLoggedIn()) {
    policy::BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    const User* logged_in_user = UserManager::Get()->GetLoggedInUser();
    if (connector->GetUserAffiliation(logged_in_user->email()) ==
            policy::USER_AFFILIATION_MANAGED) {
      VLOG(1) << "Respecting proxy for network " << network->name()
              << ", as logged-in user belongs to the domain the device "
              << "is enrolled to.";
      return false;
    }
  }

  return !GetUseSharedProxies();
}

void ProxyConfigServiceImpl::DetermineEffectiveConfig(const Network* network,
                                                      bool activate) {
  // Get prefs proxy config if available.
  net::ProxyConfig pref_config;
  ProxyPrefs::ConfigState pref_state = GetProxyConfig(&pref_config);

  // Get network proxy config if available.
  net::ProxyConfig network_config;
  net::ProxyConfigService::ConfigAvailability network_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  bool ignore_proxy = activate;
  if (network) {
    // If we're activating proxy, ignore proxy if necessary;
    // otherwise, for ui, get actual proxy to show user.
    ignore_proxy = activate ? IgnoreProxy(network) : false;
    // If network is shared but use-shared-proxies is off, use direct mode.
    if (ignore_proxy) {
      VLOG(1) << "Shared network && !use-shared-proxies, use direct";
      network_availability = net::ProxyConfigService::CONFIG_VALID;
    } else if (!network->proxy_config().empty()) {
      // Network is private or shared with user using shared proxies.
      if (ParseProxyConfig(network, &network_config)) {
        VLOG(1) << this << ": using network proxy: "
                << network->proxy_config();
        network_availability = net::ProxyConfigService::CONFIG_VALID;
      }
    }
  }

  // Determine effective proxy config, either from prefs or network.
  ProxyPrefs::ConfigState effective_config_state;
  net::ProxyConfig effective_config;
  GetEffectiveProxyConfig(pref_state, pref_config,
                          network_availability, network_config, ignore_proxy,
                          &effective_config_state, &effective_config);

  // Determine if we should activate effective proxy and which proxy config to
  // store it.
  if (activate) {  // Activate effective proxy and store into |active_config_|.
    // If last update didn't complete, we definitely update now.
    bool update_now = update_pending();
    if (!update_now) {  // Otherwise, only update now if there're changes.
      update_now = active_config_state_ != effective_config_state ||
          (active_config_state_ != ProxyPrefs::CONFIG_UNSET &&
           !active_config_.Equals(effective_config));
    }
    if (update_now) {  // Activate and store new effective config.
      active_config_state_ = effective_config_state;
      if (active_config_state_ != ProxyPrefs::CONFIG_UNSET)
        active_config_ = effective_config;
      // If effective config is from system (i.e. network), it's considered a
      // special kind of prefs that ranks below policy/extension but above
      // others, so bump it up to CONFIG_OTHER_PRECEDE to force its precedence
      // when PrefProxyConfigTrackerImpl pushes it to ChromeProxyConfigService.
      if (effective_config_state == ProxyPrefs::CONFIG_SYSTEM)
        effective_config_state = ProxyPrefs::CONFIG_OTHER_PRECEDE;
      // If config is manual, add rule to bypass local host.
      if (effective_config.proxy_rules().type !=
          net::ProxyConfig::ProxyRules::TYPE_NO_RULES)
        effective_config.proxy_rules().bypass_rules.AddRuleToBypassLocal();
      PrefProxyConfigTrackerImpl::OnProxyConfigChanged(effective_config_state,
                                                       effective_config);
      if (VLOG_IS_ON(1) && !update_pending()) {  // Update was successful.
        scoped_ptr<DictionaryValue> config_dict(static_cast<DictionaryValue*>(
            effective_config.ToValue()));
        std::string config_value;
        JSONStringValueSerializer serializer(&config_value);
        serializer.Serialize(*config_dict.get());
        VLOG(1) << this << ": Proxy changed: "
                << ConfigStateToString(active_config_state_)
                << ", " << config_value;
      }
    }
  } else {  // For UI, store effective proxy into |current_ui_config_|.
    current_ui_config_.FromNetProxyConfig(effective_config);
    current_ui_config_.state = effective_config_state;
    if (PrefPrecedes(effective_config_state)) {
      current_ui_config_.user_modifiable = false;
    } else if (!IsNetworkProxySettingsEditable(network)) {
      // TODO(xiyuan): Figure out the right way to set config state for managed
      // network.
      current_ui_config_.state = ProxyPrefs::CONFIG_POLICY;
      current_ui_config_.user_modifiable = false;
    } else {
      current_ui_config_.user_modifiable = !network || !IgnoreProxy(network);
    }
  }
}

void ProxyConfigServiceImpl::OnUISetCurrentNetwork(const Network* network) {
  DetermineEffectiveConfig(network, false);
  VLOG(1) << "Current ui network: "
          << (network->name().empty() ? current_ui_network_ : network->name())
          << ", " << ModeToString(current_ui_config_.mode)
          << ", " << ConfigStateToString(current_ui_config_.state)
          << ", modifiable:" << current_ui_config_.user_modifiable;
  // Notify whoever is interested in this change.
  std::vector<base::Closure>::iterator iter = callbacks_.begin();
  while (iter != callbacks_.end()) {
    if (iter->is_null()) {
      iter = callbacks_.erase(iter);
    } else {
      iter->Run();
      ++iter;
    }
  }
}

void ProxyConfigServiceImpl::ResetUICache() {
  current_ui_network_.clear();
  current_ui_config_ = ProxyConfig();
}

void ProxyConfigServiceImpl::FetchProxyPolicy() {
  if (CrosSettingsProvider::TRUSTED !=
      CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&ProxyConfigServiceImpl::FetchProxyPolicy,
                     pointer_factory_.GetWeakPtr()))) {
    return;
  }

  std::string policy_value;
  if (!CrosSettings::Get()->GetString(kSettingProxyEverywhere,
                                      &policy_value)) {
    VLOG(1) << "No proxy configuration exists in the device settings.";
    device_config_.clear();
    return;
  }

  VLOG(1) << "Retrieved proxy setting from device, value=["
          << policy_value << "]";
  ProxyConfig device_config;
  if (!device_config.DeserializeForDevice(policy_value) ||
      !device_config.SerializeForNetwork(&device_config_)) {
    LOG(WARNING) << "Can't deserialize device setting or serialize for network";
    device_config_.clear();
    return;
  }
  if (!active_network_.empty()) {
    VLOG(1) << "Try migrating device config to " << active_network_;
    SetProxyConfigForNetwork(active_network_, device_config_, true);
  }
}

}  // namespace chromeos
