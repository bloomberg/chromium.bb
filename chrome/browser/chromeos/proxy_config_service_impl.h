// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    : public base::RefCountedThreadSafe<ProxyConfigServiceImpl> {
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
  // DOMUI::GetProfile::GetChromeOSProxyConfigServiceImpl to allow user to read
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

      Source source;
      bool CanBeWrittenByUser(bool user_is_owner);
    };

    // Proxy setting for mode = direct or auto-detect or using pac script.
    struct AutomaticProxy : public Setting {
      GURL    pac_url;  // Set if proxy is using pac script.
    };

    // Proxy setting for mode = single-proxy or proxy-per-scheme.
    struct ManualProxy : public Setting {
      net::ProxyServer  server;
    };

    ProxyConfig() : mode(MODE_DIRECT) {}

    // Converts |this| to net::ProxyConfig.
    void ConvertToNetProxyConfig(net::ProxyConfig* net_config);

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
  void UISetProxyConfigToDirect();
  void UISetProxyConfigToAutoDetect();
  void UISetProxyConfigToPACScript(const GURL& url);
  void UISetProxyConfigToSingleProxy(const net::ProxyServer& server);
  void UISetProxyConfigToProxyPerScheme(const std::string& scheme,
                                        const net::ProxyServer& server);
  // Only valid for MODE_SINGLE_PROXY or MODE_PROXY_PER_SCHEME.
  void UISetProxyConfigBypassRules(const net::ProxyBypassRules& bypass_rules);

 private:
  friend class base::RefCountedThreadSafe<ProxyConfigServiceImpl>;

  // Called from UI thread from the various UISetProxyConfigTo*
  void OnUISetProxyConfig();

  // Posted from UI thread to IO thread to carry the new config information.
  void IOSetProxyConfig(const ProxyConfig& new_config);

  // Checks that method is called on ChromeThread::IO thread.
  void CheckCurrentlyOnIOThread();

  // Checks that method is called on ChromeThread::UI thread.
  void CheckCurrentlyOnUIThread();

  // Data members.

  // Cached proxy configuration, to be converted to net::ProxyConfig and
  // returned by IOGetProxyConfig.
  // Initially populated from the UI thread, but afterwards only accessed from
  // the IO thread.
  ProxyConfig cached_config_;

  // Copy of the proxy configuration kept on the UI thread of the last seen
  // proxy config, so as to avoid posting a call to SetNewProxyConfig when we
  // are called by UI to set new proxy but the config has not actually changed.
  ProxyConfig reference_config_;

  ObserverList<net::ProxyConfigService::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
