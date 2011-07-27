// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_server.h"

namespace chromeos {

// Implementation of proxy config service for chromeos that:
// - is RefCountedThreadSafe
// - is wrapped by chromeos::ProxyConfigService which implements
//   net::ProxyConfigService interface by fowarding the methods to this class
// - retrieves initial system proxy configuration from cros settings persisted
//   on chromeos device from chromeos revisions before migration to flimflam,
// - persists proxy setting per network in flimflim
// - provides network stack with latest proxy configuration for currently
//   active network for use on IO thread
// - provides UI with methods to retrieve and modify proxy configuration for
//   any network (either currently active or non-active) on UI thread
class ProxyConfigServiceImpl
    : public base::RefCountedThreadSafe<ProxyConfigServiceImpl>,
      public SignedSettings::Delegate<std::string>,
      public NetworkLibrary::NetworkManagerObserver,
      public NetworkLibrary::NetworkObserver {
 public:
  // ProxyConfigServiceImpl is created on the UI thread in
  // chrome/browser/net/proxy_service_factory.cc::CreateProxyConfigService
  // via BrowserProcess::chromeos_proxy_config_service_impl, and stored in
  // g_browser_process as a scoped_refptr (because it's RefCountedThreadSafe).
  //
  // Past that point, it can be accessed from the IO or UI threads.
  //
  // From the IO thread, it is accessed periodically through the wrapper class
  // chromeos::ProxyConfigService via net::ProxyConfigService interface
  // (GetLatestProxyConfig, AddObserver, RemoveObserver).
  //
  // From the UI thread, it is accessed via
  // BrowserProcess::chromeos_proxy_config_service_impl to allow user to read
  // or modify the proxy configuration via UIGetProxyConfig or
  // UISetProxyConfigTo* respectively.
  // The new modified proxy config is posted to the IO thread through
  // SetNewProxyConfig().  We then notify observers on the IO thread of the
  // configuration change.

  // In contrary to other platforms which simply use the systems' UI to allow
  // users to configure proxies, we have to implement our own UI on the chromeos
  // device.  This requires extra and specific UI requirements that
  // net::ProxyConfig does not suffice.  So we create an augmented analog to
  // net:ProxyConfig here to include and handle these UI requirements, e.g.
  // - where configuration was picked up from - policy or owner
  // - the read/write access of a proxy setting
  // - may add more stuff later.
  // This is then converted to the common net::ProxyConfig before being returned
  // to ProxyService::GetLatestProxyConfig on the IO thread to be used on the
  // network stack.
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

    // Specifies where proxy configuration was picked up from.
    enum Source {
      SOURCE_NONE,    // No default configuration.
      SOURCE_POLICY,  // Configuration is from policy.
      SOURCE_OWNER,   // Configuration is from owner.
    };

    struct Setting {
      Setting() : source(SOURCE_NONE) {}
      bool CanBeWrittenByUser(bool user_is_owner);

      Source source;
    };

    // Proxy setting for mode = direct or auto-detect or using pac script.
    struct AutomaticProxy : public Setting {
      GURL    pac_url;  // Set if proxy is using pac script.
    };

    // Proxy setting for mode = single-proxy or proxy-per-scheme.
    struct ManualProxy : public Setting {
      net::ProxyServer  server;
    };

    ProxyConfig();
    ~ProxyConfig();

    // Converts |this| to net::ProxyConfig.
    void ToNetProxyConfig(net::ProxyConfig* net_config);

    // Returns true if proxy config can be written by user.
    // If mode is MODE_PROXY_PER_SCHEME, |scheme| is one of "http", "https",
    // "ftp" or "socks"; otherwise, it should be empty or will be ignored.
    bool CanBeWrittenByUser(bool user_is_owner, const std::string& scheme);

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

    // Deserializes from string property in flimflam for a network into a
    // ProxyConfigDictionary and then into the config.
    // Opposite of SerializeForNetwork.
    bool DeserializeForNetwork(const std::string& input);

    // Returns true if the given config is equivalent to this config.
    bool Equals(const ProxyConfig& other) const;

    // Creates a textual dump of the configuration.
    std::string ToString() const;

    Mode mode;

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

    // Converts |this| to Dictionary of ProxyConfigDictionary format (which
    // is the same format used by prefs).
    DictionaryValue* ToPrefProxyConfig();

    // Converts |this| from Dictionary of ProxyConfigDictionary format.
    bool FromPrefProxyConfig(const DictionaryValue* proxy_dict);
  };

  // Usual constructor.
  ProxyConfigServiceImpl();
  // Constructor for testing.
  // |init_config| specifies the ProxyConfig to use for initialization.
  explicit ProxyConfigServiceImpl(const ProxyConfig& init_config);
  virtual ~ProxyConfigServiceImpl();

  // Methods called on IO thread from wrapper class chromeos::ProxyConfigService
  // as ProxyConfigService methods.
  void AddObserver(net::ProxyConfigService::Observer* observer);
  void RemoveObserver(net::ProxyConfigService::Observer* observer);
  // Called from GetLatestProxyConfig.
  net::ProxyConfigService::ConfigAvailability IOGetProxyConfig(
      net::ProxyConfig* config);

  // Called from UI thread to retrieve proxy configuration in |config|.
  void UIGetProxyConfig(ProxyConfig* config);

  // Called from UI thread to set service path of network to be displayed or
  // edited.  Subsequent UISet* methods will use this network, until UI calls
  // it again with a different network.
  bool UISetCurrentNetwork(const std::string& current_network);

  // Called from UI thread to make the currently active network the one to be
  // displayed or edited. Subsequent UISet* methods will use this network. until
  // UI calls it again when the active network has changed.
  bool UIMakeActiveNetworkCurrent();

  // Called from UI thread to get name of the current active network.
  const std::string& current_network_name() const {
    return current_ui_network_name_;
  }

  // Called from UI thread to set/get user preference use_shared_proxies.
  void UISetUseSharedProxies(bool use_shared);
  bool use_shared_proxies() const {
    return use_shared_proxies_;
  }

  // Called from UI thread to update proxy configuration for different modes.
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

  // Implementation for SignedSettings::Delegate
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     std::string value);

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* cros);

  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(NetworkLibrary* cros, const Network* network);

#if defined(UNIT_TEST)
  void SetTesting() {
    testing_ = true;
    active_network_ = "test";
    UIMakeActiveNetworkCurrent();
  }
#endif  // defined(UNIT_TEST)

 private:
  friend class base::RefCountedThreadSafe<ProxyConfigServiceImpl>;

  // Called from UI thread from the various UISetProxyConfigTo*.
  void OnUISetProxyConfig();

  // Posted from UI thread to IO thread to carry the new config information.
  void IOSetProxyConfig(
      const ProxyConfig& new_config,
      net::ProxyConfigService::ConfigAvailability new_availability);

  // Called from OnNetworkManagerChanged and OnNetworkChanged for currently
  // active network, to handle previously active network, new active network,
  // and if necessary, migrates device settings to flimflam and/or activates
  // proxy setting of new network.
  void OnActiveNetworkChanged(NetworkLibrary* cros,
                              const Network* active_network);

  // Sets proxy config for |network_path| into flimflam and activates setting
  // if the network is currently active.
  void SetProxyConfigForNetwork(const std::string& network_path,
                                const std::string& value,
                                bool only_set_if_empty);

  // Determines and activates proxy config of |network| based on if network is
  // shared/private or user is using shared proxies, etc.
  void DetermineConfigFromNetwork(const Network* network);

  // Set |current_ui_network_name_| with name of |network|.
  void SetCurrentNetworkName(const Network* network);

  // Checks that method is called on BrowserThread::IO thread.
  void CheckCurrentlyOnIOThread();

  // Checks that method is called on BrowserThread::UI thread.
  void CheckCurrentlyOnUIThread();

  // Data members.

  // True if running unit_tests, which will need to specifically exclude
  // flimflam logic.
  bool testing_;

  // True if tasks can be posted, which can only happen if constructor has
  // completed (NewRunnableMethod cannot be created for a RefCountedThreadBase's
  // method until the class's ref_count is at least one).
  bool can_post_task_;

  // Availability status of the configuration, initialized on UI thread, but
  // afterwards only accessed from IO thread.
  net::ProxyConfigService::ConfigAvailability config_availability_;

  // Service path of currently active network (determined via flimflam
  // notifications) whose proxy config is taking effect.
  std::string active_network_;
  // Proxy configuration of |active_network_|, only accessed from UI thread.
  ProxyConfig active_config_;

  // Proxy config retreived from device, in format generated from
  // SerializeForNetwork, that can be directly set into flimflam.
  std::string device_config_;

  // Cached proxy configuration, to be converted to net::ProxyConfig and
  // returned by IOGetProxyConfig.
  // Initially populated from UI thread, but afterwards only accessed from IO
  // thread.
  ProxyConfig cached_config_;

  // Service path of network whose proxy configuration is being displayed or
  // edited via UI, separate from |active_network_| which may be same or
  // different.
  std::string current_ui_network_;

  // Name of network with current_ui_network_, set in UIMakeActiveNetworkCurrent
  // and UISetCurrentNetwork.
  std::string current_ui_network_name_;

  // Proxy configuration of |current_ui_network|.
  ProxyConfig current_ui_config_;

  // True if user preference UseSharedProxies is true.
  bool use_shared_proxies_;

  // List of observers for changes in proxy config.
  ObserverList<net::ProxyConfigService::Observer> observers_;

  // Operation to retrieve proxy setting from device.
  scoped_refptr<SignedSettings> retrieve_property_op_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
