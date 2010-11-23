// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/network_config_view.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/options/cellular_config_view.h"
#include "chrome/browser/chromeos/options/ip_config_view.h"
#include "chrome/browser/chromeos/options/wifi_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

using views::WidgetGtk;

namespace chromeos {

NetworkConfigView::NetworkConfigView(const EthernetNetwork* ethernet)
    : browser_mode_(true),
      flags_(FLAG_ETHERNET | FLAG_SHOW_IPCONFIG),
      ethernet_(new EthernetNetwork(*ethernet)),
      cellularconfig_view_(NULL),
      wificonfig_view_(NULL),
      ipconfig_view_(NULL),
      delegate_(NULL) {
}

NetworkConfigView::NetworkConfigView(const WifiNetwork* wifi, bool login_only)
    : browser_mode_(true),
      flags_(FLAG_WIFI),
      wifi_(new WifiNetwork(*wifi)),
      cellularconfig_view_(NULL),
      wificonfig_view_(NULL),
      ipconfig_view_(NULL),
      delegate_(NULL) {
  if (login_only)
    flags_ |= FLAG_LOGIN_ONLY;
  else
    flags_ |= FLAG_SHOW_IPCONFIG;
}

NetworkConfigView::NetworkConfigView(const CellularNetwork* cellular)
    : browser_mode_(true),
      flags_(FLAG_CELLULAR | FLAG_SHOW_IPCONFIG),
      cellular_(new CellularNetwork(*cellular)),
      cellularconfig_view_(NULL),
      wificonfig_view_(NULL),
      ipconfig_view_(NULL),
      delegate_(NULL) {
}

NetworkConfigView::NetworkConfigView()
    : browser_mode_(true),
      flags_(FLAG_WIFI | FLAG_LOGIN_ONLY | FLAG_OTHER_NETWORK),
      cellularconfig_view_(NULL),
      wificonfig_view_(NULL),
      ipconfig_view_(NULL),
      delegate_(NULL) {
}

NetworkConfigView::~NetworkConfigView() {
}

gfx::NativeWindow NetworkConfigView::GetNativeWindow() const {
  return GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
}

std::wstring NetworkConfigView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK) {
    if (flags_ & FLAG_LOGIN_ONLY)
      return l10n_util::GetString(IDS_OPTIONS_SETTINGS_CONNECT);
  }
  return std::wstring();
}

bool NetworkConfigView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {
  // For login dialogs, disable ok button if nothing entered in text fields.
  if (flags_ & FLAG_LOGIN_ONLY && button == MessageBoxFlags::DIALOGBUTTON_OK)
    return wificonfig_view_->can_login();
  return true;
}

bool NetworkConfigView::Cancel() {
  if (delegate_)
    delegate_->OnDialogCancelled();
  if (flags_ & FLAG_WIFI)
    wificonfig_view_->Cancel();
  return true;
}

bool NetworkConfigView::Accept() {
  bool result = true;
  if (flags_ & FLAG_CELLULAR) {
    result = cellularconfig_view_->Save();
  }
  if (flags_ & FLAG_WIFI) {
    if (flags_ & FLAG_LOGIN_ONLY)
      result = wificonfig_view_->Login();
    else
      result = wificonfig_view_->Save();
  }
  if (result && delegate_)
    delegate_->OnDialogAccepted();
  return result;
}

std::wstring NetworkConfigView::GetWindowTitle() const {
  if (flags_ & FLAG_OTHER_NETWORK)
    return l10n_util::GetString(IDS_OPTIONS_SETTINGS_OTHER_NETWORKS);
  if (flags_ & FLAG_WIFI)
    return ASCIIToWide(wifi_->name());
  if (flags_ & FLAG_CELLULAR)
    return ASCIIToWide(cellular_->name());
  return l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
}

void NetworkConfigView::TabSelectedAt(int index) {
}

void NetworkConfigView::SetLoginTextfieldFocus() {
  if (wificonfig_view_)
    wificonfig_view_->FocusFirstField();
}

void NetworkConfigView::Layout() {
  static const int kDialogBottomPadding = 7;
  tabs_->SetBounds(0, 0, width(), height() - kDialogBottomPadding);
}

gfx::Size NetworkConfigView::GetPreferredSize() {
  // TODO(chocobo): Once UI is finalized, create locale settings.
  gfx::Size result(views::Window::GetLocalizedContentsSize(
      IDS_IMPORT_DIALOG_WIDTH_CHARS,
      IDS_IMPORT_DIALOG_HEIGHT_LINES));
  result.set_height(0);  // IMPORT_DIALOG height is too large
  // Expand the default size to fit results if necessary
  if (cellularconfig_view_) {
    gfx::Size s = cellularconfig_view_->GetPreferredSize();
    s.set_height(s.height() + kPanelVertMargin * 2);
    s.set_width(s.width() + kPanelHorizMargin * 2);
    result = gfx::Size(std::max(result.width(), s.width()),
                       std::max(result.height(), s.height()));
  } else if (wificonfig_view_) {
    gfx::Size s = wificonfig_view_->GetPreferredSize();
    s.set_height(s.height() + kPanelVertMargin * 2);
    s.set_width(s.width() + kPanelHorizMargin * 2);
    result = gfx::Size(std::max(result.width(), s.width()),
                       std::max(result.height(), s.height()));
  }
  if (ipconfig_view_) {
    gfx::Size s = ipconfig_view_->GetPreferredSize();
    s.set_height(s.height() + kPanelVertMargin * 2);
    s.set_width(s.width() + kPanelHorizMargin * 2);
    result = gfx::Size(std::max(result.width(), s.width()),
                       std::max(result.height(), s.height()));
  }
  return result;
}

void NetworkConfigView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

void NetworkConfigView::Init() {
  tabs_ = new views::TabbedPane();
  tabs_->SetListener(this);
  AddChildView(tabs_);

  if (flags_ & FLAG_CELLULAR) {
    cellularconfig_view_ = new CellularConfigView(this, cellular_.get());
    tabs_->AddTab(
        l10n_util::GetString(IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USAGE),
        cellularconfig_view_);
  }
  if (flags_ & FLAG_WIFI) {
    if (flags_ & FLAG_OTHER_NETWORK)
      wificonfig_view_ = new WifiConfigView(this);
    else
      wificonfig_view_ = new WifiConfigView(this, wifi_.get());
    tabs_->AddTab(
        l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_NETWORK_CONFIG),
        wificonfig_view_);
  }

  if (flags_ & FLAG_SHOW_IPCONFIG) {
    if (flags_ & FLAG_WIFI)
      ipconfig_view_ = new IPConfigView(wifi_->device_path());
    else if (flags_ & FLAG_CELLULAR)
      ipconfig_view_ = new IPConfigView(cellular_->device_path());
    else
      ipconfig_view_ = new IPConfigView(ethernet_->device_path());
    tabs_->AddTab(
        l10n_util::GetString(IDS_OPTIONS_SETTINGS_SECTION_TITLE_IP_CONFIG),
        ipconfig_view_);
  }
}

}  // namespace chromeos
