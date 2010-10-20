// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
class InternetOptionsHandler : public OptionsPageUIHandler,
                               public chromeos::NetworkLibrary::Observer {
 public:
  InternetOptionsHandler();
  virtual ~InternetOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(chromeos::NetworkLibrary* obj);
  virtual void CellularDataPlanChanged(chromeos::NetworkLibrary* obj);

 private:
  // Passes data needed to show details overlay for network.
  // |args| will be [ network_type, service_path, command ]
  // And command is one of 'options', 'connect', disconnect', or 'forget'
  void ButtonClickCallback(const ListValue* args);
  // Initiates cellular plan data refresh. The results from libcros will
  // be passed through CellularDataPlanChanged() callback method.
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

  bool is_certificate_in_pkcs11(const std::string& path);

  // Populates the ui with the details of the given device path. This forces
  // an overlay to be displayed in the UI.
  void PopulateDictionaryDetails(const chromeos::Network& net,
                                 chromeos::NetworkLibrary* cros);

  void PopupWirelessPassword(const chromeos::WifiNetwork& network);

  // Converts CellularDataPlan structure into dictionary for JS. Formats
  // plan settings into human readable texts.
  DictionaryValue* CellularDataPlanToDictionary(
      const chromeos::CellularDataPlan& plan);
  // Evaluates cellular plans status and returns warning string if it is near
  // expiration.
  string16 GetPlanWarning(
      const chromeos::CellularDataPlan& plan);
  // Creates the map of a network
  ListValue* GetNetwork(const std::string& service_path, const SkBitmap& icon,
      const std::string& name, bool connecting, bool connected,
      int connection_type, bool remembered);

  // Creates the map of wired networks
  ListValue* GetWiredList();
  // Creates the map of wireless networks
  ListValue* GetWirelessList();
  // Creates the map of remembered networks
  ListValue* GetRememberedList();

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_
