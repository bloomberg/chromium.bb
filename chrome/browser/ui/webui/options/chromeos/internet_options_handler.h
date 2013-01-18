// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace views {
class WidgetDelegate;
}

namespace options {

// ChromeOS internet options page UI handler.
class InternetOptionsHandler
  : public OptionsPageUIHandler,
    public chromeos::NetworkLibrary::NetworkManagerObserver,
    public chromeos::NetworkLibrary::NetworkObserver {
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

  // Handle various network commands and clicks on a network item
  // in the network list.
  // |args| has to be [ network_type, service_path, command ]
  // and command is one of the strings
  //  options, connect disconnect, activate, forget, add
  void NetworkCommandCallback(const base::ListValue* args);

  // Helper functions called by NetworkCommandCallback(...)
  void AddConnection(chromeos::ConnectionType type);
  void ConnectToNetwork(chromeos::Network* network);

  // Used to finish up async connection to the |network|.  |network| cannot
  // be NULL.
  void DoConnect(chromeos::Network* network);

  void SetCellularButtonsVisibility(
      const chromeos::CellularNetwork* cellular,
      base::DictionaryValue* dictionary,
      const std::string& carrier_id);

  void SetServerHostnameCallback(const base::ListValue* args);
  void SetPreferNetworkCallback(const base::ListValue* args);
  void SetAutoConnectCallback(const base::ListValue* args);
  void SetSharedCallback(const base::ListValue* args);
  void SetIPConfigCallback(const base::ListValue* args);
  void EnableWifiCallback(const base::ListValue* args);
  void DisableWifiCallback(const base::ListValue* args);
  void EnableCellularCallback(const base::ListValue* args);
  void DisableCellularCallback(const base::ListValue* args);
  void EnableWimaxCallback(const base::ListValue* args);
  void DisableWimaxCallback(const base::ListValue* args);
  void BuyDataPlanCallback(const base::ListValue* args);
  void SetApnCallback(const base::ListValue* args);
  void SetCarrierCallback(const base::ListValue* args);
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
  // an overlay to be displayed in the UI. Called after the asynchronous
  // request for Shill's service properties.
  void PopulateDictionaryDetailsCallback(
      const std::string& service_path,
      const base::DictionaryValue* shill_properties);
  void PopulateIPConfigsCallback(
      const std::string& service_path,
      base::DictionaryValue* shill_properties,
      const chromeos::NetworkIPConfigVector& ipconfigs,
      const std::string& hardware_address);
  void PopulateConnectionDetails(const chromeos::Network* network,
                                 base::DictionaryValue* dictionary);
  void PopulateWifiDetails(const chromeos::WifiNetwork* wifi,
                           base::DictionaryValue* dictionary);
  void PopulateWimaxDetails(const chromeos::WimaxNetwork* wimax,
                            base::DictionaryValue* dictionary);
  void PopulateCellularDetails(const chromeos::CellularNetwork* cellular,
                               base::DictionaryValue* dictionary);

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
  // Updates the display of network connection information for the details page
  // if it's up.
  void UpdateConnectionData(const chromeos::Network* network);
  // Updates the carrier change status.
  void UpdateCarrier();
  // Adds observers for wireless networks, if any, so that we can dynamically
  // display the correct icon for that network's signal strength and, in the
  // case of cellular networks, network technology and roaming status.
  void MonitorNetworks();

  // Callback for SetCarrier to notify once it's complete.
  void CarrierStatusCallback(
      const std::string& path,
      chromeos::NetworkMethodErrorType error,
      const std::string& error_message);

  // Retrieves a data url for a resource.
  std::string GetIconDataUrl(int resource_id) const;

  // Convenience pointer to netwrok library (will not change).
  chromeos::NetworkLibrary* cros_;

  content::NotificationRegistrar registrar_;

  // Weak pointer factory so we can start connections at a later time
  // without worrying that they will actually try to happen after the lifetime
  // of this object.
  base::WeakPtrFactory<InternetOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
