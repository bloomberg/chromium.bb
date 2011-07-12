// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_dropdown_button.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"

namespace {

const int kThrobDuration = 750;

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton

NetworkDropdownButton::NetworkDropdownButton(bool is_browser_mode,
                                             gfx::NativeWindow parent_window,
                                             bool should_show_options)
    : DropDownButton(NULL,
                     UTF16ToWide(l10n_util::GetStringUTF16(
                         IDS_STATUSBAR_NO_NETWORKS_MESSAGE)),
                     this,
                     true),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)),
      parent_window_(parent_window),
      should_show_options_(should_show_options),
      last_network_type_(TYPE_WIFI) {
  network_menu_.reset(new NetworkMenu(this, is_browser_mode));
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(ui::Tween::LINEAR);
  CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
  // The initial state will be updated on Refresh.
  // See network_selection_view.cc.
}

NetworkDropdownButton::~NetworkDropdownButton() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, ui::AnimationDelegate implementation:

void NetworkDropdownButton::AnimationProgressed(
    const ui::Animation* animation) {
  if (animation == &animation_connecting_) {
    SetIcon(*NetworkMenu::IconForNetworkConnecting(
        animation_connecting_.GetCurrentValue(), last_network_type_));
    SchedulePaint();
  } else {
    MenuButton::AnimationProgressed(animation);
  }
}

void NetworkDropdownButton::Refresh() {
  OnNetworkManagerChanged(CrosLibrary::Get()->GetNetworkLibrary());
}

void NetworkDropdownButton::SetFirstLevelMenuWidth(int width) {
  network_menu_->set_min_width(width);
}

void NetworkDropdownButton::CancelMenu() {
  network_menu_->CancelMenu();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, NetworkMenu::Delegate implementation:

views::MenuButton* NetworkDropdownButton::GetMenuButton() {
  return this;
}

gfx::NativeWindow NetworkDropdownButton::GetNativeWindow() const {
  return parent_window_;
}

void NetworkDropdownButton::OpenButtonOptions() {
  if (proxy_settings_dialog_.get() == NULL) {
    proxy_settings_dialog_.reset(new ProxySettingsDialog(this,
                                                         GetNativeWindow()));
  }
  proxy_settings_dialog_->Show();
}

bool NetworkDropdownButton::ShouldOpenButtonOptions() const {
  return should_show_options_;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, views::ViewMenuDelegate implementation:

void NetworkDropdownButton::RunMenu(views::View* source, const gfx::Point& pt) {
  network_menu_->RunMenu(source);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, overridden from views::View.

void NetworkDropdownButton::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, LoginHtmlDialog::Delegate implementation:

void NetworkDropdownButton::OnDialogClosed() {
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, NetworkLibrary::NetworkManagerObserver implementation:

void NetworkDropdownButton::OnNetworkManagerChanged(NetworkLibrary* cros) {
  // Show network that we will actually use. It could be another network than
  // user selected. For example user selected WiFi network but we have Ethernet
  // connection and Chrome OS device will actually use Ethernet.

  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (CrosLibrary::Get()->EnsureLoaded()) {
    // Always show the active network, if any
    const Network* active_network = cros->active_network();
    last_network_type_ = NetworkMenu::TypeForNetwork(active_network);
    if (active_network != NULL) {
      animation_connecting_.Stop();
      if (active_network->type() == TYPE_ETHERNET) {
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
        SetText(UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET)));
      } else if (active_network->type() == TYPE_WIFI) {
        const WifiNetwork* wifi =
            static_cast<const WifiNetwork*>(active_network);
        SetIcon(*NetworkMenu::IconForNetworkStrength(wifi));
        SetText(UTF8ToWide(wifi->name()));
      } else if (active_network->type() == TYPE_CELLULAR) {
        const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(active_network);
        SetIcon(*NetworkMenu::IconForNetworkStrength(cellular));
        SetText(UTF8ToWide(cellular->name()));
      } else {
        NOTREACHED();
      }
    } else if (cros->wifi_connecting() || cros->cellular_connecting()) {
      if (!animation_connecting_.is_animating()) {
        animation_connecting_.Reset();
        animation_connecting_.StartThrobbing(-1);
        SetIcon(*NetworkMenu::IconForNetworkConnecting(0, last_network_type_));
      }
      if (cros->wifi_connecting())
        SetText(UTF8ToWide(cros->wifi_network()->name()));
      else if (cros->cellular_connecting())
        SetText(UTF8ToWide(cros->cellular_network()->name()));
    }

    if (!cros->Connected() && !cros->Connecting()) {
      animation_connecting_.Stop();
      SetIcon(SkBitmap());
      SetText(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_NONE)));
    }
  } else {
    animation_connecting_.Stop();
    SetIcon(SkBitmap());
    SetText(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE)));
  }

  SchedulePaint();
  network_menu_->UpdateMenu();
}

}  // namespace chromeos
