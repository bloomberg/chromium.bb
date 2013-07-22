// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_PROXY_CONFIG_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_UI_PROXY_CONFIG_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/ui_proxy_config.h"

class PrefService;

namespace chromeos {

class NetworkState;

// This class is only accessed from the UI via Profile::GetProxyConfigTracker to
// allow the user to read and modify the proxy configuration via
// GetProxyConfig and SetProxyConfig.
//
// Before reading/setting a proxy config, a network has to be selected using
// either SetCurrentNetwork (any remembered network) or
// MakeActiveNetworkCurrent.
class UIProxyConfigService {
 public:
  UIProxyConfigService();
  ~UIProxyConfigService();

  // After this call, proxy settings are read from |profile_prefs| and
  // |local_state_prefs|. In case of usage for the sign-in screen,
  // |profile_prefs| must be NULL because sign-in screen should depend only on
  // shared settings.
  void SetPrefs(PrefService* profile_prefs, PrefService* local_state_prefs);

  // Called by UI to set the network with service path |current_network| to be
  // displayed or edited.  Subsequent Set*/Get* methods will use this
  // network, until this method is called again.
  void SetCurrentNetwork(const std::string& current_network);

  void UpdateFromPrefs();

  // Called from UI to retrieve the stored proxy configuration, which is either
  // the last proxy config of the current network or the one last set by
  // SetProxyConfig.
  void GetProxyConfig(UIProxyConfig* config) const;

  // Called from UI to update proxy configuration for different modes. Stores
  // and persists |config| to shill for the current network.
  void SetProxyConfig(const UIProxyConfig& config);

 private:
  // Determines effective proxy config based on prefs from config tracker,
  // |network| and if user is using shared proxies.  The effective config is
  // stored in |current_ui_config_| but not activated on network stack, and
  // hence, not picked up by observers.
  void DetermineEffectiveConfig(const NetworkState& network);

  // Service path of network whose proxy configuration is being displayed or
  // edited via UI.
  std::string current_ui_network_;

  // Proxy configuration of |current_ui_network_|.
  UIProxyConfig current_ui_config_;

  bool signin_screen_;

  // Not owned.
  PrefService* profile_prefs_;

  // Not owned.
  PrefService* local_state_prefs_;

  DISALLOW_COPY_AND_ASSIGN(UIProxyConfigService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_PROXY_CONFIG_SERVICE_H_
