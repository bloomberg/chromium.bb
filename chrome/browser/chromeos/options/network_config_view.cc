// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/network_config_view.h"

#include <algorithm>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/options/vpn_config_view.h"
#include "chrome/browser/chromeos/options/wifi_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

using views::WidgetGtk;

namespace chromeos {

// static
const int ChildNetworkConfigView::kPassphraseWidth = 150;

NetworkConfigView::NetworkConfigView(Network* network)
    : browser_mode_(true),
      delegate_(NULL) {
  if (network->type() == TYPE_WIFI) {
    child_config_view_ =
        new WifiConfigView(this, static_cast<WifiNetwork*>(network));
  } else if (network->type() == TYPE_VPN) {
    child_config_view_ =
        new VPNConfigView(this, static_cast<VirtualNetwork*>(network));
  } else {
    NOTREACHED();
    child_config_view_ = NULL;
  }
}

NetworkConfigView::NetworkConfigView(ConnectionType type)
    : browser_mode_(true),
      delegate_(NULL) {
  if (type == TYPE_WIFI) {
    child_config_view_ = new WifiConfigView(this);
  } else if (type == TYPE_VPN) {
    child_config_view_ = new VPNConfigView(this);
  } else {
    NOTREACHED();
    child_config_view_ = NULL;
  }
}

gfx::NativeWindow NetworkConfigView::GetNativeWindow() const {
  return
      GTK_WINDOW(static_cast<const WidgetGtk*>(GetWidget())->GetNativeView());
}

std::wstring NetworkConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_CONNECT));
  return std::wstring();
}

bool NetworkConfigView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  // Disable connect button if cannot login.
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return child_config_view_->CanLogin();
  return true;
}

bool NetworkConfigView::Cancel() {
  if (delegate_)
    delegate_->OnDialogCancelled();
  child_config_view_->Cancel();
  return true;
}

bool NetworkConfigView::Accept() {
  bool result = child_config_view_->Login();
  if (result && delegate_)
    delegate_->OnDialogAccepted();
  return result;
}

std::wstring NetworkConfigView::GetWindowTitle() const {
  return UTF16ToWide(child_config_view_->GetTitle());
}

void NetworkConfigView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name =
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_WIFI_NETWORKS);
  state->role = ui::AccessibilityTypes::ROLE_DIALOG;
}

void NetworkConfigView::Layout() {
  child_config_view_->SetBounds(0, 0, width(), height());
}

gfx::Size NetworkConfigView::GetPreferredSize() {
  gfx::Size result(views::Window::GetLocalizedContentsSize(
      IDS_JOIN_WIFI_NETWORK_DIALOG_WIDTH_CHARS,
      IDS_JOIN_WIFI_NETWORK_DIALOG_MINIMUM_HEIGHT_LINES));
  gfx::Size size = child_config_view_->GetPreferredSize();
  result.set_height(size.height());
  if (size.width() > result.width())
    result.set_width(size.width());
  return result;
}

void NetworkConfigView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this) {
    AddChildView(child_config_view_);
    child_config_view_->InitFocus();
  }
}

}  // namespace chromeos
