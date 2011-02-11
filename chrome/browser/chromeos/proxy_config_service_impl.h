// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
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
//   on chromeos device
// - provides network stack with latest system proxy configuration for use on
//   IO thread
// - provides UI with methods to retrieve and modify system proxy configuration
//   on UI thread
// - TODO(kuan): persists proxy configuration settings on chromeos device using
//   cros settings
class ProxyConfigServiceImpl
    : public base::RefCountedThreadSafe<ProxyConfigServiceImpl>,
      public SignedSettings::Delegate<bool>,
      public SignedSettings::Delegate<std::string> {
 public:
  // ProxyConfigServiceImpl is created on the UI thread in
  // chrome/browser/net/chrome_url_request_context.cc::CreateProxyConfigService
  // via ProfileImpl::GetChromeOSProxyConfigServiceImpl, and stored in Profile
  // as a scoped_refptr (because it's RefCountedThreadSafe).
  //
  // Past that point, it can be accessed from the IO or UI threads.
  //
  // From the IO thread, it is accessed periodically through the wrapper class
  // chromeos::ProxyConfigService via net::ProxyConfigService interface
  // (GetLatestProxyConfig, AddObserver, RemoveObserver).
  //
  // From the UI thread, it is accessed via
  // WebUI::GetProfile::GetChromeOSProxyConfigServiceImpl to allow user to read
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
      virtual DictionaryValue* Encode() const;
      bool Decode(DictionaryValue* dict);

      Source source;
    };

    // Proxy setting for mode = direct or auto-detect or using pac script.
    struct AutomaticProxy : public Setting {
      virtual DictionaryValue* Encode() const;
      bool Decode(DictionaryValue* dict, Mode mode);

      GURL    pac_url;  // Set if proxy is using pac script.
    };

    // Proxy setting for mode = single-proxy or proxy-per-scheme.
    struct ManualProxy : public Setting {
      virtual DictionaryValue* Encode() const;
      bool Decode(DictionaryValue* dict, net::ProxyServer::Scheme scheme);

      net::ProxyServer  server;
    };

    ProxyConfig() : mode(MODE_DIRECT) {}

    // Converts |this| to net::ProxyConfig.
    void ToNetProxyConfig(net::ProxyConfig* net_config);

    // Returns true if proxy config can be written by user.
    // If mode is MODE_PROXY_PER_SCHEME, |scheme| is one of "http", "https",
    // "ftp" or "socks"; otherwise, it should be empty or will be ignored.
    bool CanBeWrittenByUser(bool user_is_owner, const std::string& scheme);

    // Map |scheme| (one of "http", "https", "ftp" or "socks") to the correct
    // ManualProxy.  Returns NULL if scheme is invalid.
    ManualProxy* MapSchemeToProxy(const std::string& scheme);

    // Serializes config into a DictionaryValue and then into std::string
    // persisted as property on device.
    bool Serialize(std::string* output);
    // Deserializes from property value on device as std::string into a
    // DictionaryValue and then into the config.  Opposite of Serialize.
    bool Deserialize(const std::string& input);

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
    // Encodes |manual_proxy| and adds it as value into |key_name| of |dict|.
    void EncodeManualProxy(const ManualProxy& manual_proxy,
        DictionaryValue* dict, const char* key_name);
    // Decodes value of |key_name| in |dict| into |manual_proxy| with |scheme|;
    // if |ok_if_absent| is true, function returns true if |key_name| doesn't
    // exist in |dict|.
    bool DecodeManualProxy(DictionaryValue* dict, const char* key_name,
        bool ok_if_absent, net::ProxyServer::Scheme scheme,
        ManualProxy* manual_proxy);
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
  bool IOGetProxyConfig(net::ProxyConfig* config);

  // Called from UI thread to retrieve proxy configuration in |config|.
  void UIGetProxyConfig(ProxyConfig* config);

  // Called from UI thread to update proxy configuration for different modes.
  // Returns true if config is set properly and config service has proceeded to
  // start activating it on network stack and persisting it to device.
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
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     bool value);

 private:
  friend class base::RefCountedThreadSafe<ProxyConfigServiceImpl>;

  // Init proxy to default config, i.e. AutoDetect.
  // If |post_to_io_thread| is true, a task will be posted to IO thread to
  // update |cached_config|.
  void InitConfigToDefault(bool post_to_io_thread);

  // Persists proxy config to device.
  void PersistConfigToDevice();

  // Called from UI thread from the various UISetProxyConfigTo*
  // |update_to_device| is true to persist new proxy config to device.
  void OnUISetProxyConfig(bool update_to_device);

  // Posted from UI thread to IO thread to carry the new config information.
  void IOSetProxyConfig(const ProxyConfig& new_config);

  // Checks that method is called on BrowserThread::IO thread.
  void CheckCurrentlyOnIOThread();

  // Checks that method is called on BrowserThread::UI thread.
  void CheckCurrentlyOnUIThread();

  // Data members.

  // True if tasks can be posted, which can only happen if constructor has
  // completed (NewRunnableMethod cannot be created for a RefCountedThreadBase's
  // method until the class's ref_count is at least one).
  bool can_post_task_;

  // True if config has been fetched from device or initialized properly.
  bool has_config_;

  // True if there's a pending operation to store proxy setting to device.
  bool persist_to_device_pending_;

  // Cached proxy configuration, to be converted to net::ProxyConfig and
  // returned by IOGetProxyConfig.
  // Initially populated from the UI thread, but afterwards only accessed from
  // the IO thread.
  ProxyConfig cached_config_;

  // Copy of the proxy configuration kept on the UI thread of the last seen
  // proxy config, so as to avoid posting a call to SetNewProxyConfig when we
  // are called by UI to set new proxy but the config has not actually changed.
  ProxyConfig reference_config_;

  // List of observers for changes in proxy config.
  ObserverList<net::ProxyConfigService::Observer> observers_;

  // Operations to retrieve and store proxy setting from and to device
  // respectively.
  scoped_refptr<SignedSettings> retrieve_property_op_;
  scoped_refptr<SignedSettings> store_property_op_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
