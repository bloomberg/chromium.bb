// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_

#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/system/system_clock_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
enum class LoginStatus;
}

namespace views {
class Widget;
class WidgetDelegate;
}

// Handles method calls delegated back to chrome from ash. Also notifies ash of
// relevant state changes in chrome.
// TODO: Consider renaming this to SystemTrayClientChromeOS.
class SystemTrayClient : public ash::mojom::SystemTrayClient,
                         public chromeos::system::SystemClockObserver {
 public:
  SystemTrayClient();
  ~SystemTrayClient() override;

  static SystemTrayClient* Get();

  // Returns the login state based on the user type, lock screen status, etc.
  static ash::LoginStatus GetUserLoginStatus();

  // Returns the container id for the parent window for new dialogs. The parent
  // varies based on the current login and lock screen state.
  static int GetDialogParentContainerId();

  // Creates a modal dialog in the parent window for new dialogs on the primary
  // display. See GetDialogParentContainerId() and views::CreateDialogWidget().
  // The returned widget is owned by its native widget.
  static views::Widget* CreateUnownedDialogWidget(
      views::WidgetDelegate* widget_delegate);

  // ash::mojom::SystemTrayClient:
  void ShowSettings() override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowDisplaySettings() override;
  void ShowPowerSettings() override;
  void ShowChromeSlow() override;
  void ShowIMESettings() override;
  void ShowHelp() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowPaletteHelp() override;
  void ShowPaletteSettings() override;
  void ShowPublicAccountInfo() override;
  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkCreate(const std::string& type) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  void ShowProxySettings() override;
  void SignOut() override;
  void RequestRestartForUpdate() override;

 private:
  // chromeos::system::SystemClockObserver:
  void OnSystemClockChanged(chromeos::system::SystemClock* clock) override;

  // Connects or reconnects the |system_tray_| interface.
  void ConnectToSystemTray();

  // Handles errors on the |system_tray_| interface connection.
  void OnClientConnectionError();

  // System tray mojo service in ash.
  ash::mojom::SystemTrayPtr system_tray_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayClient);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CLIENT_H_
