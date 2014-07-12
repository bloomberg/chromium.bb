// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class PrefService;

namespace chromeos {
class DeviceState;
class NetworkState;
class NetworkStateHandlerObserver;
}

namespace gfx {
class ImageSkia;
}

namespace views {
class WidgetDelegate;
}

namespace chromeos {
namespace options {

// ChromeOS internet options page UI handler.
class InternetOptionsHandler
    : public ::options::OptionsPageUIHandler,
      public chromeos::NetworkStateHandlerObserver,
      public content::NotificationObserver {
 public:
  InternetOptionsHandler();
  virtual ~InternetOptionsHandler();

 private:
  // OptionsPageUIHandler
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // WebUIMessageHandler (from OptionsPageUIHandler)
  virtual void RegisterMessages() OVERRIDE;

  // Callbacks to set network state properties.
  void EnableWifiCallback(const base::ListValue* args);
  void DisableWifiCallback(const base::ListValue* args);
  void EnableCellularCallback(const base::ListValue* args);
  void DisableCellularCallback(const base::ListValue* args);
  void EnableWimaxCallback(const base::ListValue* args);
  void DisableWimaxCallback(const base::ListValue* args);
  void ShowMorePlanInfoCallback(const base::ListValue* args);
  void BuyDataPlanCallback(const base::ListValue* args);
  void SetApnCallback(const base::ListValue* args);
  void SetApnProperties(const base::ListValue* args,
                        const std::string& service_path,
                        const base::DictionaryValue& shill_properties);
  void CarrierStatusCallback();
  void SetCarrierCallback(const base::ListValue* args);
  void SetSimCardLockCallback(const base::ListValue* args);
  void ChangePinCallback(const base::ListValue* args);
  void RefreshNetworksCallback(const base::ListValue* args);

  // Retrieves a data url for a resource.
  std::string GetIconDataUrl(int resource_id) const;

  // Refreshes the display of network information.
  void RefreshNetworkData();

  // Updates the display of network connection information for the details page
  // if visible.
  void UpdateConnectionData(const std::string& service_path);
  void UpdateConnectionDataCallback(
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);
  // Called when carrier data has been updated to informs the JS.
  void UpdateCarrier();

  // NetworkStateHandlerObserver
  virtual void DeviceListChanged() OVERRIDE;
  virtual void NetworkListChanged() OVERRIDE;
  virtual void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkPropertiesUpdated(
      const chromeos::NetworkState* network) OVERRIDE;

  // Updates the logged in user type.
  void UpdateLoggedInUserType();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Additional callbacks to set network state properties.
  void SetServerHostnameCallback(const base::ListValue* args);
  void SetPreferNetworkCallback(const base::ListValue* args);
  void SetAutoConnectCallback(const base::ListValue* args);
  void SetIPConfigCallback(const base::ListValue* args);
  void SetIPConfigProperties(const base::ListValue* args,
                             const std::string& service_path,
                             const base::DictionaryValue& shill_properties);

  // Retrieves the properties for |service_path| and calls showDetailedInfo
  // with the results.
  void PopulateDictionaryDetailsCallback(
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);

  // Gets the native window for hosting dialogs, etc.
  gfx::NativeWindow GetNativeWindow() const;

  // Gets the UI scale factor.
  float GetScaleFactor() const;

  // Gets the user PrefService associated with the WebUI.
  const PrefService* GetPrefs() const;

  // Handle various network commands and clicks on a network item
  // in the network list.
  // |args| must be { network_type, service_path, command } with 'command'
  // one of: [ add, forget, options, connect disconnect, activate ]
  void NetworkCommandCallback(const base::ListValue* args);

  // Helper functions called by NetworkCommandCallback(...)
  void AddConnection(const std::string& type);

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

  content::NotificationRegistrar registrar_;

  // Keep track of the service path for the network shown in the Details view.
  std::string details_path_;

  // Weak pointer factory so we can start connections at a later time
  // without worrying that they will actually try to happen after the lifetime
  // of this object.
  base::WeakPtrFactory<InternetOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_H_
