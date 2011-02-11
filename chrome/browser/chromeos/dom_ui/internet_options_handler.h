// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/dom_ui/options/options_ui.h"

class SkBitmap;
namespace views {
class WindowDelegate;
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

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* network_lib);
  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* network_lib,
                                const chromeos::Network* network);
  // NetworkLibrary::CellularDataPlanObserver implementation.
  virtual void OnCellularDataPlanChanged(chromeos::NetworkLibrary* network_lib);

 private:
  // Opens a modal popup dialog.
  void CreateModalPopup(views::WindowDelegate* view);

  // Passes data needed to show details overlay for network.
  // |args| will be [ network_type, service_path, command ]
  // And command is one of 'options', 'connect', disconnect', 'activate' or
  // 'forget'
  // Handle{Wifi,Cellular}ButtonClick handles button click on a wireless
  // network item and a cellular network item respectively.
  void ButtonClickCallback(const ListValue* args);
  void HandleWifiButtonClick(const std::string& service_path,
                             const std::string& command);
  void HandleCellularButtonClick(const std::string& service_path,
                                 const std::string& command);

  // Initiates cellular plan data refresh. The results from libcros will be
  // passed through CellularDataPlanChanged() callback method.
  // |args| will be [ service_path ]
  void RefreshCellularPlanCallback(const ListValue* args);

  void LoginCallback(const ListValue* args);
  void LoginCertCallback(const ListValue* args);
  void LoginToOtherCallback(const ListValue* args);
  void SetDetailsCallback(const ListValue* args);
  void EnableWifiCallback(const ListValue* args);
  void DisableWifiCallback(const ListValue* args);
  void EnableCellularCallback(const ListValue* args);
  void DisableCellularCallback(const ListValue* args);
  void BuyDataPlanCallback(const ListValue* args);

  // Parses 'path' to determine if the certificate is stored in a pkcs#11
  // device. flimflam recognizes the string "SETTINGS:" to specify
  // authentication parameters. 'key_id=' indicates that the certificate is
  // stored in a pkcs#11 device.
  // See src/third_party/flimflam/files/doc/service-api.txt.
  bool IsCertificateInPkcs11(const std::string& path);

  // Populates the ui with the details of the given device path. This forces
  // an overlay to be displayed in the UI.
  void PopulateDictionaryDetails(const chromeos::Network* net,
                                 chromeos::NetworkLibrary* cros);
  void PopulateWifiDetails(DictionaryValue* dictionary,
                           const chromeos::WifiNetwork* wifi);
  void PopulateCellularDetails(DictionaryValue* dictionary,
                               const chromeos::CellularNetwork*
                               cellular);

  // Converts CellularDataPlan structure into dictionary for JS. Formats plan
  // settings into human readable texts.
  DictionaryValue* CellularDataPlanToDictionary(
      const chromeos::CellularDataPlan* plan);
  // Creates the map of a network.
  ListValue* GetNetwork(const std::string& service_path,
                        const SkBitmap& icon,
                        const std::string& name,
                        bool connecting,
                        bool connected,
                        bool connectable,
                        chromeos::ConnectionType connection_type,
                        bool remembered,
                        chromeos::ActivationState activation_state,
                        bool restricted_ip);

  // Creates the map of wired networks.
  ListValue* GetWiredList();
  // Creates the map of wireless networks.
  ListValue* GetWirelessList();
  // Creates the map of remembered networks.
  ListValue* GetRememberedList();
  // Fills network information into JS dictionary for displaying network lists.
  void FillNetworkInfo(DictionaryValue* dictionary,
                       chromeos::NetworkLibrary* cros);
  // Refreshes the display of network information.
  void RefreshNetworkData(chromeos::NetworkLibrary* cros);
  // Adds an observer for the active network, if any, so that we can dynamically
  // display the correct icon for that network's signal strength.
  void MonitorActiveNetwork(chromeos::NetworkLibrary* cros);

  // If any network is currently active, this is its service path.
  std::string active_network_;

  // A boolean flag of whether to use WebUI for connect UI. True to use WebUI
  // and false to use Views dialogs.
  bool use_settings_ui_;

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_
