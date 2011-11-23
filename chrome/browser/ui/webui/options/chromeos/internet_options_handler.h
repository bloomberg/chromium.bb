// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class SkBitmap;
namespace views {
class WidgetDelegate;
}

// ChromeOS internet options page UI handler.
class InternetOptionsHandler
  : public OptionsPageUIHandler,
    public chromeos::NetworkLibrary::NetworkManagerObserver,
    public chromeos::NetworkLibrary::NetworkObserver,
    public chromeos::NetworkLibrary::CellularDataPlanObserver {
 public:
  InternetOptionsHandler();
  virtual ~InternetOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(
      chromeos::NetworkLibrary* network_lib) OVERRIDE;
  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* network_lib,
                                const chromeos::Network* network) OVERRIDE;
  // NetworkLibrary::CellularDataPlanObserver implementation.
  virtual void OnCellularDataPlanChanged(
      chromeos::NetworkLibrary* network_lib) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Opens a modal popup dialog.
  void CreateModalPopup(views::WidgetDelegate* view);
  gfx::NativeWindow GetNativeWindow() const;

  // Passes data needed to show details overlay for network.
  // |args| will be [ network_type, service_path, command ]
  // And command is one of 'options', 'connect', disconnect', 'activate' or
  // 'forget'
  // Handle{Wifi,Cellular}ButtonClick handles button click on a wireless
  // network item and a cellular network item respectively.
  void ButtonClickCallback(const base::ListValue* args);
  void HandleWifiButtonClick(const std::string& service_path,
                             const std::string& command);
  void HandleCellularButtonClick(const std::string& service_path,
                                 const std::string& command);
  void HandleVPNButtonClick(const std::string& service_path,
                            const std::string& command);

  // Initiates cellular plan data refresh. The results from libcros will be
  // passed through CellularDataPlanChanged() callback method.
  // |args| will be [ service_path ]
  void RefreshCellularPlanCallback(const base::ListValue* args);
  void SetActivationButtonVisibility(
      const chromeos::CellularNetwork* cellular,
      base::DictionaryValue* dictionary);

  void SetPreferNetworkCallback(const base::ListValue* args);
  void SetAutoConnectCallback(const base::ListValue* args);
  void SetSharedCallback(const base::ListValue* args);
  void SetIPConfigCallback(const base::ListValue* args);
  void EnableWifiCallback(const base::ListValue* args);
  void DisableWifiCallback(const base::ListValue* args);
  void EnableCellularCallback(const base::ListValue* args);
  void DisableCellularCallback(const base::ListValue* args);
  void ImportNetworkSettingsCallback(const base::ListValue* args);
  void BuyDataPlanCallback(const base::ListValue* args);
  void SetApnCallback(const base::ListValue* args);
  void SetSimCardLockCallback(const base::ListValue* args);
  void ChangePinCallback(const base::ListValue* args);
  void ShareNetworkCallback(const base::ListValue* args);

  // Populates the ui with the details of the given device path. This forces
  // an overlay to be displayed in the UI.
  void PopulateDictionaryDetails(const chromeos::Network* network);
  void PopulateWifiDetails(const chromeos::WifiNetwork* wifi,
                           base::DictionaryValue* dictionary);
  void PopulateCellularDetails(const chromeos::CellularNetwork* cellular,
                               base::DictionaryValue* dictionary);
  void PopulateVPNDetails(const chromeos::VirtualNetwork* vpn,
                          base::DictionaryValue* dictionary);

  // Converts CellularDataPlan structure into dictionary for JS. Formats plan
  // settings into human readable texts.
  base::DictionaryValue* CellularDataPlanToDictionary(
      const chromeos::CellularDataPlan* plan);

  // Converts CellularApn stuct into dictionary for JS.
  base::DictionaryValue* CreateDictionaryFromCellularApn(
      const chromeos::CellularApn& apn);

  // Creates the map of a network.
  base::ListValue* GetNetwork(const std::string& service_path,
                              const SkBitmap& icon,
                              const std::string& name,
                              bool connecting,
                              bool connected,
                              bool connectable,
                              chromeos::ConnectionType connection_type,
                              bool remembered,
                              bool shared,
                              chromeos::ActivationState activation_state,
                              bool restricted_ip);

  // Creates the map of wired networks.
  base::ListValue* GetWiredList();
  // Creates the map of wireless networks.
  base::ListValue* GetWirelessList();
  // Creates the map of virtual networks.
  base::ListValue* GetVPNList();
  // Creates the map of remembered networks.
  base::ListValue* GetRememberedList();
  // Fills network information into JS dictionary for displaying network lists.
  void FillNetworkInfo(base::DictionaryValue* dictionary);
  // Refreshes the display of network information.
  void RefreshNetworkData();
  // Adds observers for wireless networks, if any, so that we can dynamically
  // display the correct icon for that network's signal strength and, in the
  // case of cellular networks, network technology and roaming status.
  void MonitorNetworks();

  // Convenience pointer to netwrok library (will not change).
  chromeos::NetworkLibrary* cros_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
