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
#include "ui/gfx/rect.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/text_button.h"

namespace chromeos {

// static
const int ChildNetworkConfigView::kInputFieldMinWidth = 270;

NetworkConfigView::NetworkConfigView(Network* network)
    : delegate_(NULL),
      advanced_button_(NULL),
      advanced_button_container_(NULL) {
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
    : delegate_(NULL),
      advanced_button_(NULL),
      advanced_button_container_(NULL) {
  if (type == TYPE_WIFI) {
    child_config_view_ = new WifiConfigView(this, false /* show_8021x */);
    CreateAdvancedButton();
  } else if (type == TYPE_VPN) {
    child_config_view_ = new VPNConfigView(this);
  } else {
    NOTREACHED();
    child_config_view_ = NULL;
  }
}

gfx::NativeWindow NetworkConfigView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

string16 NetworkConfigView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_CONNECT);
  return string16();
}

bool NetworkConfigView::IsDialogButtonEnabled(ui::DialogButton button) const {
  // Disable connect button if cannot login.
  if (button == ui::DIALOG_BUTTON_OK)
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
  // Do not attempt login if it is guaranteed to fail, keep the dialog open.
  if (!child_config_view_->CanLogin())
    return false;
  bool result = child_config_view_->Login();
  if (result && delegate_)
    delegate_->OnDialogAccepted();
  return result;
}

views::View* NetworkConfigView::GetExtraView() {
  return advanced_button_container_;
}

bool NetworkConfigView::IsModal() const {
  return true;
}

views::View* NetworkConfigView::GetContentsView() {
  return this;
}

string16 NetworkConfigView::GetWindowTitle() const {
  return child_config_view_->GetTitle();
}

void NetworkConfigView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name =
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_OTHER_WIFI_NETWORKS);
  state->role = ui::AccessibilityTypes::ROLE_DIALOG;
}

void NetworkConfigView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  if (advanced_button_ && sender == advanced_button_) {
    advanced_button_->SetVisible(false);
    ShowAdvancedView();
  }
}

void NetworkConfigView::ShowAdvancedView() {
  // Clear out the old widgets and build new ones.
  RemoveChildView(child_config_view_);
  delete child_config_view_;
  // For now, there is only an advanced view for Wi-Fi 802.1X.
  child_config_view_ = new WifiConfigView(this, true /* show_8021x */);
  AddChildView(child_config_view_);
  // Resize the window to be able to hold the new widgets.
  gfx::Size size = views::Widget::GetLocalizedContentsSize(
      IDS_JOIN_WIFI_NETWORK_DIALOG_ADVANCED_WIDTH_CHARS,
      IDS_JOIN_WIFI_NETWORK_DIALOG_ADVANCED_MINIMUM_HEIGHT_LINES);
  // Get the new bounds with desired size at the same center point.
  gfx::Rect bounds = GetWidget()->GetWindowScreenBounds();
  int horiz_padding = bounds.width() - size.width();
  int vert_padding = bounds.height() - size.height();
  bounds.Inset(horiz_padding / 2, vert_padding / 2,
               horiz_padding / 2, vert_padding / 2);
  GetWidget()->SetBoundsConstrained(bounds);
  Layout();
  child_config_view_->InitFocus();
}

void NetworkConfigView::Layout() {
  child_config_view_->SetBounds(0, 0, width(), height());
}

gfx::Size NetworkConfigView::GetPreferredSize() {
  gfx::Size result(views::Widget::GetLocalizedContentsSize(
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

void NetworkConfigView::CreateAdvancedButton() {
  advanced_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADVANCED_BUTTON));

  // Wrap the advanced button in a grid layout in order to left-align it.
  advanced_button_container_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(advanced_button_container_);
  advanced_button_container_->SetLayoutManager(layout);

  int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                        0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_set_id);
  layout->AddView(advanced_button_);
}

}  // namespace chromeos
