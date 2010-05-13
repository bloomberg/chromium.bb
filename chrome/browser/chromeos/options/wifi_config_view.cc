// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

// The width of the password field.
const int kPasswordWidth = 150;

WifiConfigView::WifiConfigView(NetworkConfigView* parent, WifiNetwork wifi)
    : parent_(parent),
      other_network_(false),
      can_login_(false),
      wifi_(wifi),
      ssid_textfield_(NULL),
      identity_textfield_(NULL),
      certificate_browse_button_(NULL),
      certificate_path_(),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      autoconnect_checkbox_(NULL) {
  Init();
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent)
    : parent_(parent),
      other_network_(true),
      can_login_(false),
      ssid_textfield_(NULL),
      identity_textfield_(NULL),
      certificate_browse_button_(NULL),
      certificate_path_(),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      autoconnect_checkbox_(NULL) {
  Init();
}

void WifiConfigView::UpdateCanLogin(void) {
  bool can_login = true;
  if (other_network_) {
    // Since the user can try to connect to a non-encrypted hidden network,
    // only enforce ssid is non-empty.
    can_login = !ssid_textfield_->text().empty();
  } else {
    // Connecting to an encrypted network, so make sure passphrase is non-empty.
    can_login = !passphrase_textfield_->text().empty();
    if (identity_textfield_ != NULL)
      can_login = !identity_textfield_->text().empty() &&
          !certificate_path_.empty();
  }

  // Update the login button enable/disable state if can_login_ changes.
  if (can_login != can_login_) {
    can_login_ = can_login;
    parent_->GetDialogClientView()->UpdateDialogButtons();
  }
}

void WifiConfigView::ContentsChanged(views::Textfield* sender,
                                     const string16& new_contents) {
  UpdateCanLogin();
}

void WifiConfigView::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  if (sender == passphrase_visible_button_) {
    if (passphrase_textfield_)
      passphrase_textfield_->SetPassword(!passphrase_textfield_->IsPassword());
  } else if (sender == certificate_browse_button_) {
    select_file_dialog_ = SelectFileDialog::Create(this);
    select_file_dialog_->set_browser_mode(parent_->is_browser_mode());
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                    string16(), FilePath(), NULL, 0,
                                    std::string(),
                                    parent_->is_browser_mode() ?
                                        NULL :
                                        parent_->GetNativeWindow(),
                                    NULL);
  } else {
    NOTREACHED();
  }
}

void WifiConfigView::FileSelected(const FilePath& path,
                                   int index, void* params) {
  certificate_path_ = path;
  certificate_browse_button_->SetLabel(path.BaseName().ToWStringHack());
  UpdateCanLogin();  // TODO(njw) Check if the passphrase decrypts the key.
}

bool WifiConfigView::Login() {
  string16 identity_string, certificate_path_string;

  if (identity_textfield_ != NULL) {
    identity_string = identity_textfield_->text();
    certificate_path_string = WideToUTF16(certificate_path_.ToWStringHack());
  }
  if (other_network_) {
    CrosLibrary::Get()->GetNetworkLibrary()->ConnectToWifiNetwork(
        ssid_textfield_->text(), passphrase_textfield_->text(),
        identity_string, certificate_path_string,
        autoconnect_checkbox_->checked());
  } else {
    Save();
    CrosLibrary::Get()->GetNetworkLibrary()->ConnectToWifiNetwork(
        wifi_, passphrase_textfield_->text(),
        identity_string, certificate_path_string);
  }
  return true;
}

bool WifiConfigView::Save() {
  // Save password and auto-connect here.
  if (!other_network_) {
    bool changed = false;

    bool auto_connect = autoconnect_checkbox_->checked();
    if (auto_connect != wifi_.auto_connect()) {
      wifi_.set_auto_connect(auto_connect);
      changed = true;
    }

    if (passphrase_textfield_) {
      const std::string& passphrase =
          UTF16ToUTF8(passphrase_textfield_->text());
      if (passphrase != wifi_.passphrase()) {
        wifi_.set_passphrase(passphrase);
        changed = true;
      }
    }

    if (changed)
      CrosLibrary::Get()->GetNetworkLibrary()->SaveWifiNetwork(wifi_);
  }
  return true;
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
  else if (identity_textfield_)
    identity_textfield_->RequestFocus();
  else if (passphrase_textfield_)
    passphrase_textfield_->RequestFocus();
}

void WifiConfigView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  // Label
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  // Textfield
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, kPasswordWidth);
  // Password visible button
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SSID)));
  if (other_network_) {
    ssid_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    ssid_textfield_->SetController(this);
    layout->AddView(ssid_textfield_);
  } else {
    views::Label* label = new views::Label(ASCIIToWide(wifi_.name()));
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(label);
  }
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Add ID and cert password if we're using 802.1x
  // XXX we're cheating and assuming 802.1x means EAP-TLS - not true
  // in general, but very common. WPA Supplicant doesn't report the
  // EAP type because it's unknown until the process begins, and we'd
  // need some kind of callback.
  if (wifi_.encrypted() && wifi_.encryption() == SECURITY_8021X) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY)));
    identity_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_textfield_->SetController(this);
    if (!wifi_.identity().empty())
      identity_textfield_->SetText(UTF8ToUTF16(wifi_.identity()));
    layout->AddView(identity_textfield_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT)));
    if (!wifi_.cert_path().empty()) {
      certificate_path_ = FilePath(wifi_.cert_path());
      certificate_browse_button_ = new views::NativeButton(
          this, UTF8ToWide(wifi_.cert_path()));
    } else {
      certificate_browse_button_ = new views::NativeButton(
          this,
          l10n_util::GetString(
              IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_BUTTON));
    }
    layout->AddView(certificate_browse_button_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  // Add passphrase if other_network or wifi is encrypted.
  if (other_network_ || wifi_.encrypted()) {
    layout->StartRow(0, column_view_set_id);
    int label_text_id;
    if (wifi_.encryption() == SECURITY_8021X)
      label_text_id =
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PRIVATE_KEY_PASSWORD;
    else
      label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE;
    layout->AddView(new views::Label(l10n_util::GetString(label_text_id)));
    passphrase_textfield_ = new views::Textfield(
        views::Textfield::STYLE_PASSWORD);
    passphrase_textfield_->SetController(this);
    if (!wifi_.passphrase().empty())
      passphrase_textfield_->SetText(UTF8ToUTF16(wifi_.passphrase()));
    layout->AddView(passphrase_textfield_);
    // Password visible button.
    passphrase_visible_button_ = new views::ImageButton(this);
    passphrase_visible_button_->SetImage(views::ImageButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
    passphrase_visible_button_->SetImageAlignment(
        views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
    layout->AddView(passphrase_visible_button_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  // Autoconnect checkbox
  autoconnect_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
  // For other network, default to autoconnect.
  bool autoconnect = other_network_ || wifi_.auto_connect();
  autoconnect_checkbox_->SetChecked(autoconnect);
  layout->StartRow(0, column_view_set_id);
  layout->AddView(autoconnect_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
