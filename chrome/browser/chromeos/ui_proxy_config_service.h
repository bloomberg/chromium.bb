// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_PROXY_CONFIG_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_UI_PROXY_CONFIG_SERVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/ui_proxy_config.h"

class PrefService;

namespace chromeos {

class Network;
class ProxyConfigServiceImpl;

// This class is only accessed from the UI via Profile::GetProxyConfigTracker to
// allow the user to read and modify the proxy configuration via
// GetProxyConfig and SetProxyConfig.
//
// Before reading/setting a proxy config, a network has to be selected using
// either SetCurrentNetwork (any remembered network) or
// MakeActiveNetworkCurrent.
class UIProxyConfigService {
 public:
  explicit UIProxyConfigService(PrefService* pref_service);
  ~UIProxyConfigService();

  // Add/Remove callback functions for notification when network to be viewed is
  // changed by the UI.
  // Currently, these are only called by CoreChromeOSOptionsHandler.
  void AddNotificationCallback(base::Closure callback);
  void RemoveNotificationCallback(base::Closure callback);

  // Called by UI to set the network with service path |current_network| to be
  // displayed or edited.  Subsequent Set*/Get* methods will use this
  // network, until this method is called again.
  void SetCurrentNetwork(const std::string& current_network);

  // Called from UI to make the current active network the one to be displayed
  // or edited.  See SetCurrentNetwork.
  void MakeActiveNetworkCurrent();

  // Called from UI to get name of the current network.
  void GetCurrentNetworkName(std::string* network_name);

  // Called from UI to retrieve the stored proxy configuration, which is either
  // the last proxy config of the current network or the one last set by
  // SetProxyConfig.
  void GetProxyConfig(UIProxyConfig* config);

  // Called from UI to update proxy configuration for different modes. Stores
  // and persists |config| to shill for the current network.
  void SetProxyConfig(const UIProxyConfig& config);

 private:
  // Determines effective proxy config based on prefs from config tracker,
  // |network| and if user is using shared proxies.  The effective config is
  // stored in |current_ui_config_| but not activated on network stack, and
  // hence, not picked up by observers.
  void DetermineEffectiveConfig(const Network& network);

  // Service path of network whose proxy configuration is being displayed or
  // edited via UI.
  std::string current_ui_network_;

  // Proxy configuration of |current_ui_network_|.
  UIProxyConfig current_ui_config_;

  // Callbacks for notification when network to be viewed has been changed from
  // the UI.
  std::vector<base::Closure> callbacks_;

  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(UIProxyConfigService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_PROXY_CONFIG_SERVICE_H_
