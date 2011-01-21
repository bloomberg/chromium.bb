// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/network_config_view.h"

#include <algorithm>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/options/wifi_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

using views::WidgetGtk;

namespace chromeos {

NetworkConfigView::NetworkConfigView(const WifiNetwork* wifi)
    : browser_mode_(true),
      title_(ASCIIToWide(wifi->name())),
      wificonfig_view_(new WifiConfigView(this, wifi)),
      delegate_(NULL) {
}

NetworkConfigView::NetworkConfigView()
    : browser_mode_(true),
      title_(UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_NETWORKS))),
      wificonfig_view_(new WifiConfigView(this)),
      delegate_(NULL) {
}

gfx::NativeWindow NetworkConfigView::GetNativeWindow() const {
  return GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
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
    return wificonfig_view_->can_login();
  return true;
}

bool NetworkConfigView::Cancel() {
  if (delegate_)
    delegate_->OnDialogCancelled();
  wificonfig_view_->Cancel();
  return true;
}

bool NetworkConfigView::Accept() {
  bool result = wificonfig_view_->Login();
  if (result && delegate_)
    delegate_->OnDialogAccepted();
  return result;
}

std::wstring NetworkConfigView::GetWindowTitle() const {
  return title_;
}

void NetworkConfigView::Layout() {
  wificonfig_view_->SetBounds(0, 0, width(), height());
}

gfx::Size NetworkConfigView::GetPreferredSize() {
  // TODO(chocobo): Once UI is finalized, create locale settings.
  gfx::Size result(views::Window::GetLocalizedContentsSize(
      IDS_IMPORT_DIALOG_WIDTH_CHARS,
      IDS_IMPORT_DIALOG_HEIGHT_LINES));
  result.set_height(wificonfig_view_->GetPreferredSize().height());
  return result;
}

void NetworkConfigView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    AddChildView(wificonfig_view_);
}

}  // namespace chromeos
