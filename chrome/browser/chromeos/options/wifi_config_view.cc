// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
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
    // Connecting to an encrypted network
    if (passphrase_textfield_ != NULL) {
      // if the network requires a passphrase, make sure it is non empty.
      can_login &= !passphrase_textfield_->text().empty();
    }
    if (identity_textfield_ != NULL) {
      // If we have an identity field, we can login if we have a non empty
      // identity and a certificate path
      can_login &= !identity_textfield_->text().empty() &&
          !certificate_path_.empty();
    }
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

bool WifiConfigView::HandleKeystroke(
    views::Textfield* sender,
    const views::Textfield::Keystroke& keystroke) {
  if (sender == passphrase_textfield_ &&
      keystroke.GetKeyboardCode() == app::VKEY_RETURN) {
    parent_->GetDialogClientView()->AcceptWindow();
  }
  return false;
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
  certificate_path_ = path.value();
  if (certificate_browse_button_)
    certificate_browse_button_->SetLabel(path.BaseName().ToWStringHack());
  UpdateCanLogin();  // TODO(njw) Check if the passphrase decrypts the key.
}

bool WifiConfigView::Login() {
  std::string identity_string;
  if (identity_textfield_ != NULL) {
    identity_string = UTF16ToUTF8(identity_textfield_->text());
  }
  if (other_network_) {
    CrosLibrary::Get()->GetNetworkLibrary()->ConnectToWifiNetwork(
        GetSSID(), GetPassphrase(),
        identity_string, certificate_path_,
        autoconnect_checkbox_ ? autoconnect_checkbox_->checked() : true);
  } else {
    Save();
    CrosLibrary::Get()->GetNetworkLibrary()->ConnectToWifiNetwork(
        wifi_, GetPassphrase(),
        identity_string, certificate_path_);
  }
  return true;
}

bool WifiConfigView::Save() {
  // Save password and auto-connect here.
  if (!other_network_) {
    bool changed = false;

    if (autoconnect_checkbox_) {
      bool auto_connect = autoconnect_checkbox_->checked();
      if (auto_connect != wifi_.auto_connect()) {
        wifi_.set_auto_connect(auto_connect);
        changed = true;
      }
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

const std::string WifiConfigView::GetSSID() const {
  std::string result;
  if (ssid_textfield_ != NULL)
    result = UTF16ToUTF8(ssid_textfield_->text());
  return result;
}

const std::string WifiConfigView::GetPassphrase() const {
  std::string result;
  if (passphrase_textfield_ != NULL)
    result = UTF16ToUTF8(passphrase_textfield_->text());
  return result;
}

void WifiConfigView::FocusFirstField() {
  if (ssid_textfield_)
    ssid_textfield_->RequestFocus();
  else if (identity_textfield_)
    identity_textfield_->RequestFocus();
  else if (passphrase_textfield_)
    passphrase_textfield_->RequestFocus();
}

// Parse 'path' to determine if the certificate is stored in a pkcs#11 device.
// flimflam recognizes the string "SETTINGS:" to specify authentication
// parameters. 'key_id=' indicates that the certificate is stored in a pkcs#11
// device. See src/third_party/flimflam/files/doc/service-api.txt.
static bool is_certificate_in_pkcs11(const std::string& path) {
  static const std::string settings_string("SETTINGS:");
  static const std::string pkcs11_key("key_id");
  if (path.find(settings_string) == 0) {
    std::string::size_type idx = path.find(pkcs11_key);
    if (idx != std::string::npos)
      idx = path.find_first_not_of(kWhitespaceASCII, idx + pkcs11_key.length());
    if (idx != std::string::npos && path[idx] == '=')
      return true;
  }
  return false;
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
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID)));
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

  // Certificates stored in a pkcs11 device can not be browsed
  // and do not require a passphrase.
  bool certificate_in_pkcs11 = false;

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
      certificate_path_ = wifi_.cert_path();
      certificate_in_pkcs11 = is_certificate_in_pkcs11(certificate_path_);
    }
    if (certificate_in_pkcs11) {
      std::wstring label = l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_INSTALLED);
      views::Label* cert_text = new views::Label(label);
      cert_text->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      layout->AddView(cert_text);
    } else {
      std::wstring label;
      if (!certificate_path_.empty())
        label = UTF8ToWide(certificate_path_);
      else
        label = l10n_util::GetString(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_BUTTON);
      certificate_browse_button_ = new views::NativeButton(this, label);
      layout->AddView(certificate_browse_button_);
    }
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  // Add passphrase if other_network or wifi is encrypted.
  if (other_network_ || (wifi_.encrypted() && !certificate_in_pkcs11)) {
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

  // If there's an error, add an error message label.
  // Right now, only displaying bad_passphrase and bad_wepkey errors.
  if (wifi_.error() == ERROR_BAD_PASSPHRASE ||
      wifi_.error() == ERROR_BAD_WEPKEY) {
    layout->StartRow(0, column_view_set_id);
    layout->SkipColumns(1);
    int id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE;
    if (wifi_.error() == ERROR_BAD_WEPKEY)
      id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_WEPKEY;
    views::Label* label_error = new views::Label(l10n_util::GetString(id));
    label_error->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    label_error->SetColor(SK_ColorRED);
    layout->AddView(label_error);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  // Autoconnect checkbox
  // Only show if this network is already remembered (a favorite).
  if (wifi_.favorite()) {
    autoconnect_checkbox_ = new views::Checkbox(l10n_util::GetString(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
    // For other network, default to autoconnect.
    bool autoconnect = other_network_ || wifi_.auto_connect();
    autoconnect_checkbox_->SetChecked(autoconnect);
    layout->StartRow(0, column_view_set_id);
    layout->AddView(autoconnect_checkbox_, 3, 1);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }
}

}  // namespace chromeos
