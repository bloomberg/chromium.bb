// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_view.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

WifiConfigView::WifiConfigView(NetworkConfigView* parent, WifiNetwork wifi)
    : parent_(parent),
      other_network_(false),
      can_login_(false),
      wifi_(wifi),
      ssid_textfield_(NULL),
      passphrase_textfield_(NULL) {
  Init();
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent)
    : parent_(parent),
      other_network_(true),
      can_login_(false),
      ssid_textfield_(NULL),
      passphrase_textfield_(NULL) {
  Init();
}

void WifiConfigView::ContentsChanged(views::Textfield* sender,
                                     const string16& new_contents) {
  bool can_login = true;
  if (other_network_) {
    // Since the user can try to connect to a non-encrypted hidden network,
    // only enforce ssid is non-empty.
    can_login = !ssid_textfield_->text().empty();
  } else {
    // Connecting to an encrypted network, so make sure passphrase is non-empty.
    can_login = !passphrase_textfield_->text().empty();
  }

  // Update the login button enable/disable state if can_login_ changes.
  if (can_login != can_login_) {
    can_login_ = can_login;
    parent_->GetDialogClientView()->UpdateDialogButtons();
  }
}

const string16& WifiConfigView::GetSSID() const {
  return ssid_textfield_->text();
}

const string16& WifiConfigView::GetPassphrase() const {
  return passphrase_textfield_->text();
}

void WifiConfigView::FocusFirstField() {
  if (ssid_textfield_)
    ssid_textfield_->RequestFocus();
  else if (passphrase_textfield_)
    passphrase_textfield_->RequestFocus();
}

void WifiConfigView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 200);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SSID)));
  if (other_network_) {
    ssid_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    ssid_textfield_->SetController(this);
    layout->AddView(ssid_textfield_);
  } else {
    views::Label* label = new views::Label(ASCIIToWide(wifi_.ssid));
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(label);
  }
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add passphrase if other_network or wifi is encrypted.
  if (other_network_ || wifi_.encrypted) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE)));
    passphrase_textfield_ = new views::Textfield(
        views::Textfield::STYLE_PASSWORD);
    passphrase_textfield_->SetController(this);
    if (!wifi_.passphrase.empty())
      passphrase_textfield_->SetText(UTF8ToUTF16(wifi_.passphrase));
    layout->AddView(passphrase_textfield_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
}

}  // namespace chromeos
