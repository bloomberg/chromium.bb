// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

// Implementation of proxy config service for chromeos that:
// - extends PrefProxyConfigTrackerImpl (and so lives and runs entirely on UI
//   thread) to handle proxy from prefs (via PrefProxyConfigTrackerImpl) and
//   system i.e. network (via flimflam notifications)
// - exists one per profile and one per local state
// - retrieves initial system proxy configuration from cros settings persisted
//   on chromeos device from chromeos revisions before migration to flimflam,
// - persists proxy setting per network in flimflim
// - provides network stack with latest effective proxy configuration for
//   currently active network via PrefProxyConfigTrackerImpl's mechanism of
//   pushing config to ChromeProxyConfigService
// - provides UI with methods to retrieve and modify proxy configuration for
//   any remembered network (either currently active or non-active) of current
//   user profile
class ProxyConfigServiceImpl
    : public PrefProxyConfigTrackerImpl,
      public SignedSettings::Delegate<const base::Value*>,
      public NetworkLibrary::NetworkManagerObserver,
      public NetworkLibrary::NetworkObserver {
 public:
  // ProxyConfigServiceImpl is created in ProxyServiceFactory::
  // CreatePrefProxyConfigTrackerImpl via Profile::GetProxyConfigTracker() for
  // profile or IOThread constructor for local state and is owned by the
  // respective classes.
  //
  // From the UI, it is accessed via Profile::GetProxyConfigTracker to allow
  // user to read or modify the proxy configuration via UIGetProxyConfig or
  // UISetProxyConfigTo* respectively.
  // The new modified proxy config, together with proxy from prefs if available,
  // are used to determine the effective proxy config, which is then pushed
  // through PrefProxyConfigTrackerImpl to ChromeProxyConfigService to the
  // network stack.
  //
  // In contrary to other platforms which simply use the systems' UI to allow
  // users to configure proxies, we have to implement our own UI on the chromeos
  // device.  This requires extra and specific UI requirements that
  // net::ProxyConfig does not suffice.  So we create an augmented analog to
  // net:ProxyConfig here to include and handle these UI requirements, e.g.
  // - state of configuration e.g. where it was picked up from - policy,
  //   extension, etc (refer to ProxyPrefs::ConfigState)
  // - the read/write access of a proxy setting
  // - may add more stuff later.
  // This is then converted to the common net::ProxyConfig before being pushed
  // to PrefProxyConfigTrackerImpl::OnProxyConfigChanged and then to the network
  // stack.
  struct ProxyConfig {
    // Specifies if proxy config is direct, auto-detect, using pac script,
    // single-proxy, or proxy-per-scheme.
    enum Mode {
      MODE_DIRECT,
      MODE_AUTO_DETECT,
      MODE_PAC_SCRIPT,
      MODE_SINGLE_PROXY,
      MODE_PROXY_PER_SCHEME,
    };

    // Proxy setting for mode = direct or auto-detect or using pac script.
    struct AutomaticProxy {
      GURL pac_url;  // Set if proxy is using pac script.
    };

    // Proxy setting for mode = single-proxy or proxy-per-scheme.
    struct ManualProxy {
      net::ProxyServer server;
    };

    ProxyConfig();
    ~ProxyConfig();

    // Converts net::ProxyConfig to |this|.
    bool FromNetProxyConfig(const net::ProxyConfig& net_config);

    // Converts |this| to Dictionary of ProxyConfigDictionary format (which
    // is the same format used by prefs).
    DictionaryValue* ToPrefProxyConfig();

    // Map |scheme| (one of "http", "https", "ftp" or "socks") to the correct
    // ManualProxy.  Returns NULL if scheme is invalid.
    ManualProxy* MapSchemeToProxy(const std::string& scheme);

    // We've migrated device settings to flimflam, so we only need to
    // deserialize previously persisted device settings.
    // Deserializes from signed setting on device as std::string into a
    // protobuf and then into the config.
    bool DeserializeForDevice(const std::string& input);

    // Serializes config into a ProxyConfigDictionary and then std::string
    // persisted as string property in flimflam for a network.
    bool SerializeForNetwork(std::string* output);

    Mode mode;

    ProxyPrefs::ConfigState state;

    // True if user can modify proxy settings via UI.
    // If proxy is managed by policy or extension or other_precde or is for
    // shared network but kUseSharedProxies is turned off, it can't be modified
    // by user.
    bool user_modifiable;

    // Set if mode is MODE_DIRECT or MODE_AUTO_DETECT or MODE_PAC_SCRIPT.
    AutomaticProxy  automatic_proxy;
    // Set if mode is MODE_SINGLE_PROXY.
    ManualProxy     single_proxy;
    // Set if mode is MODE_PROXY_PER_SCHEME and has http proxy.
    ManualProxy     http_proxy;
    // Set if mode is MODE_PROXY_PER_SCHEME and has https proxy.
    ManualProxy     https_proxy;
    // Set if mode is MODE_PROXY_PER_SCHEME and has ftp proxy.
    ManualProxy     ftp_proxy;
    // Set if mode is MODE_PROXY_PER_SCHEME and has socks proxy.
    ManualProxy     socks_proxy;

    // Exceptions for when not to use a proxy.
    net::ProxyBypassRules  bypass_rules;

   private:
    // Encodes the proxy server as "<url-scheme>=<proxy-scheme>://<proxy>"
    static void EncodeAndAppendProxyServer(const std::string& scheme,
                                           const net::ProxyServer& server,
                                           std::string* spec);
  };

  // Constructor.
  explicit ProxyConfigServiceImpl(PrefService* pref_service);
  virtual ~ProxyConfigServiceImpl();

  // Called by UI to set service path of |network| to be displayed or edited.
  // Subsequent UISet* methods will use this network, until UI calls it again
  // with a different network.
  void UISetCurrentNetwork(const std::string& current_network);

  // Called from UI to make the currently active network the one to be displayed
  // or edited. Subsequent UISet* methods will use this network until UI calls
  // it again when the active network has changed.
  void UIMakeActiveNetworkCurrent();

  // Called from UI to get name of the current network set via
  // UISetCurrentNetwork or UIMakeActiveNetworkCurrent.
  void UIGetCurrentNetworkName(std::string* network_name);

  // Called from UI to retrieve proxy configuration in |current_ui_config_|.
  void UIGetProxyConfig(ProxyConfig* config);

  // Called from UI to update proxy configuration for different modes.
  // Returns true if config is set properly and persisted to flimflam for the
  // current network (set via UISetCurrentNetwork/UIMakeActiveNetworkCurrent).
  // If this network is also currently active, config service proceeds to start
  // activating it on network stack.
  // Returns false if config is not set properly, probably because information
  // is incomplete or invalid; while config service won't proceed to activate or
  // persist this config, the information is "cached" in the service, so that
  // the next UIGetProxyConfig call will return this latest information.
  bool UISetProxyConfigToDirect();
  bool UISetProxyConfigToAutoDetect();
  bool UISetProxyConfigToPACScript(const GURL& pac_url);
  bool UISetProxyConfigToSingleProxy(const net::ProxyServer& server);
  // |scheme| is one of "http", "https", "ftp" or "socks".
  bool UISetProxyConfigToProxyPerScheme(const std::string& scheme,
                                        const net::ProxyServer& server);
  // Only valid for MODE_SINGLE_PROXY or MODE_PROXY_PER_SCHEME.
  bool UISetProxyConfigBypassRules(const net::ProxyBypassRules& bypass_rules);

  // Add/Remove callback functions for notification when network to be viewed is
  // changed by the UI.
  void AddNotificationCallback(base::Closure callback);
  void RemoveNotificationCallback(base::Closure callback);

  // PrefProxyConfigTrackerImpl implementation.
  virtual void OnProxyConfigChanged(ProxyPrefs::ConfigState config_state,
                                    const net::ProxyConfig& config) OVERRIDE;

  // Implementation for SignedSettings::Delegate
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     const base::Value* value) OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* cros) OVERRIDE;

  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(NetworkLibrary* cros,
                                const Network* network) OVERRIDE;

  // Register UseShardProxies preference.
  static void RegisterPrefs(PrefService* pref_service);

#if defined(UNIT_TEST)
  void SetTesting(ProxyConfig* test_config) {
    UIMakeActiveNetworkCurrent();
    if (test_config) {
      std::string value;
      test_config->SerializeForNetwork(&value);
      SetProxyConfigForNetwork(active_network_, value, false);
    }
  }
#endif  // defined(UNIT_TEST)

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called from the various UISetProxyConfigTo*.
  void OnUISetProxyConfig();

  // Called from OnNetworkManagerChanged and OnNetworkChanged for currently
  // active network, to handle previously active network, new active network,
  // and if necessary, migrates device settings to flimflam and/or activates
  // proxy setting of new network.
  void OnActiveNetworkChanged(NetworkLibrary* cros,
                              const Network* active_network);

  // Sets proxy config for |network_path| into flimflam and activates setting
  // if the network is currently active.  If |only_set_if_empty| is true,
  // proxy will be set and saved only if network has no proxy.
  void SetProxyConfigForNetwork(const std::string& network_path,
                                const std::string& value,
                                bool only_set_if_empty);

  // Returns value of UseSharedProxies preference if it's not default, else
  // returns false if user is logged in and true otherwise.
  bool GetUseSharedProxies();

  // Determines effective proxy config based on prefs from config tracker,
  // |network| and if user is using shared proxies.
  // If |activate| is true, effective config is stored in |active_config_| and
  // activated on network stack, and hence, picked up by observers.
  // if |activate| is false, effective config is stored in |current_ui_config_|
  // but not activated on network stack, and hence, not picked up by observers.
  void DetermineEffectiveConfig(const Network* network, bool activate);

  // Determines |current_ui_config_| based on |network|, called from
  // UISetCurrentNetwork and UIMakeActiveNetworkActive.
  void OnUISetCurrentNetwork(const Network* network);

  // Returns true if proxy is to be ignored for network, which happens if
  // network is shared and use-shared-proxies is turned off.
  bool IgnoreProxy(const Network* network) {
    return network->profile_type() == PROFILE_SHARED && !GetUseSharedProxies();
  }

  // Reset UI cache variables that keep track of UI activities.
  void ResetUICache();

  // Data members.

  // Service path of currently active network (determined via flimflam
  // notifications); if effective proxy config is from system, proxy of this
  // network will be the one taking effect.
  std::string active_network_;

  // State of |active_config_|.  |active_config_| is only valid if
  // |active_config_state_| is not ProxyPrefs::CONFIG_UNSET.
  ProxyPrefs::ConfigState active_config_state_;

  // Active proxy configuration, which could be from prefs or network.
  net::ProxyConfig active_config_;

  // Proxy config retreived from device, in format generated from
  // SerializeForNetwork, that can be directly set into flimflam.
  std::string device_config_;

  // Service path of network whose proxy configuration is being displayed or
  // edited via UI, separate from |active_network_| which may be same or
  // different.
  std::string current_ui_network_;

  // Proxy configuration of |current_ui_network_|.
  ProxyConfig current_ui_config_;

  // Track changes in UseSharedProxies user preference.
  BooleanPrefMember use_shared_proxies_;

  content::NotificationRegistrar registrar_;

  // Operation to retrieve proxy setting from device.
  scoped_refptr<SignedSettings> retrieve_property_op_;

  // Callbacks for notification when network to be viewed has been changed from
  // the UI.
  std::vector<base::Closure> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
