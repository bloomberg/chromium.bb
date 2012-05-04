// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_INTERNET_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_INTERNET_OPTIONS_HANDLER2_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class SkBitmap;

namespace views {
class WidgetDelegate;
}

namespace options2 {

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
  virtual void InitializePage() OVERRIDE;

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

  // Returns the last active browser. If there is no such browser, creates a new
  // browser window with an empty tab and returns it.
  Browser* GetAppropriateBrowser();

  // Passes data needed to show the details overlay for a network.
  // |args| will be [ network_type, service_path, command ]
  // And command is one of 'options', 'connect', disconnect', 'activate' or
  // 'forget'
  void NetworkCommandCallback(const base::ListValue* args);

  // Handle{Wifi,Cellular,VPN}ButtonClick handles button click on a wireless,
  // cellular or VPN network item, respectively.
  void HandleWifiButtonClick(const std::string& service_path,
                             const std::string& command);
  void HandleCellularButtonClick(const std::string& service_path,
                                 const std::string& command);
  void HandleVPNButtonClick(const std::string& service_path,
                            const std::string& command);

  // Used to finish up async connection to the |network|.  |network| cannot
  // be NULL.
  void DoConnect(chromeos::Network* network);

  // Initiates cellular plan data refresh. The results from libcros will be
  // passed through CellularDataPlanChanged() callback method.
  // |args| will be [ service_path ]
  void RefreshCellularPlanCallback(const base::ListValue* args);
  void SetActivationButtonVisibility(
      const chromeos::CellularNetwork* cellular,
      base::DictionaryValue* dictionary,
      const std::string& carrier_id);

  void SetPreferNetworkCallback(const base::ListValue* args);
  void SetAutoConnectCallback(const base::ListValue* args);
  void SetSharedCallback(const base::ListValue* args);
  void SetIPConfigCallback(const base::ListValue* args);
  void EnableWifiCallback(const base::ListValue* args);
  void DisableWifiCallback(const base::ListValue* args);
  void EnableCellularCallback(const base::ListValue* args);
  void DisableCellularCallback(const base::ListValue* args);
  void BuyDataPlanCallback(const base::ListValue* args);
  void SetApnCallback(const base::ListValue* args);
  void SetSimCardLockCallback(const base::ListValue* args);
  void ChangePinCallback(const base::ListValue* args);
  void ShareNetworkCallback(const base::ListValue* args);
  void ShowMorePlanInfoCallback(const base::ListValue* args);
  void RefreshNetworksCallback(const base::ListValue* args);

  /**
   * Toggle airplane mode.  Disables all wireless networks when activated.
   * Celluar and Bluetooth connections remain disabled while active, but
   * Wi-Fi can be reactivated. |args| is unused.
   */
  void ToggleAirplaneModeCallback(const ListValue* args);

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

  // Stores a dictionary under |key| in |settings| that is suitable to be sent
  // to the webui that contains the actual value of a setting and whether it's
  // controlled by policy. Takes ownership of |value|.
  void SetValueDictionary(DictionaryValue* settings,
                          const char* key,
                          base::Value* value,
                          const chromeos::NetworkPropertyUIData& ui_data);

  // Convenience pointer to netwrok library (will not change).
  chromeos::NetworkLibrary* cros_;

  content::NotificationRegistrar registrar_;

  // Weak pointer factory so we can start connections at a later time
  // without worrying that they will actually try to happen after the lifetime
  // of this object.
  base::WeakPtrFactory<InternetOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_INTERNET_OPTIONS_HANDLER2_H_
