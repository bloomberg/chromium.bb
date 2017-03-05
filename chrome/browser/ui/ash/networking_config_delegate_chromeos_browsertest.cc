// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"

#include "ash/common/login_status.h"
#include "ash/common/system/chromeos/network/network_detailed_view.h"
#include "ash/common/system/chromeos/network/tray_network.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/wm_shell.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace {

// Returns true if this view or any child view has the given tooltip.
bool HasChildWithTooltip(views::View* view,
                         const base::string16& given_tooltip) {
  base::string16 tooltip;
  view->GetTooltipText(gfx::Point(), &tooltip);
  if (tooltip == given_tooltip)
    return true;

  for (int i = 0; i < view->child_count(); ++i) {
    if (HasChildWithTooltip(view->child_at(i), given_tooltip))
      return true;
  }

  return false;
}

using NetworkingConfigDelegateChromeosTest = ExtensionBrowserTest;

// Tests that an extension registering itself as handling a Wi-Fi SSID updates
// the ash system tray network item.
IN_PROC_BROWSER_TEST_F(NetworkingConfigDelegateChromeosTest, SystemTrayItem) {
  // Load the extension and wait for the background page script to run. This
  // registers the extension as the network config handler for wifi1.
  ExtensionTestMessageListener listener("done", false);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("networking_config_delegate")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Open the system tray menu.
  ash::SystemTray* system_tray =
      ash::WmShell::Get()->GetPrimaryRootWindowController()->GetSystemTray();
  system_tray->ShowDefaultView(ash::BUBBLE_CREATE_NEW);
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(system_tray->HasSystemBubble());

  // Show the network detail view.
  ash::TrayNetwork* tray_network = system_tray->GetTrayNetworkForTesting();
  system_tray->ShowDetailedView(tray_network, 0, false, ash::BUBBLE_CREATE_NEW);
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(tray_network->detailed());

  // Look for an item with a tooltip saying it is an extension-controlled
  // network. Searching all children allows this test to avoid knowing about the
  // specifics of the view hierarchy.
  base::string16 expected_tooltip = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16("NetworkingConfigDelegate test extension"));
  EXPECT_TRUE(HasChildWithTooltip(tray_network->detailed(), expected_tooltip));

  // Close the system tray menu.
  system_tray->CloseSystemBubble();
  content::RunAllPendingInMessageLoop();
}

}  // namespace
