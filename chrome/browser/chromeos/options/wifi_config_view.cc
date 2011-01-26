// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

namespace chromeos {

// The width of the password field.
const int kPasswordWidth = 150;

enum SecurityComboboxIndex {
  INDEX_NONE  = 0,
  INDEX_WEP   = 1,
  INDEX_WPA   = 2,
  INDEX_RSN   = 3,
  INDEX_COUNT = 4
};

int WifiConfigView::SecurityComboboxModel::GetItemCount() {
  return INDEX_COUNT;
}

string16 WifiConfigView::SecurityComboboxModel::GetItemAt(int index) {
  if (index == INDEX_NONE)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_NONE);
  else if (index == INDEX_WEP)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_WEP);
  else if (index == INDEX_WPA)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_WPA);
  else if (index == INDEX_RSN)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_RSN);
  NOTREACHED();
  return string16();
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent,
                               const WifiNetwork* wifi)
    : parent_(parent),
      can_login_(false),
      wifi_(new WifiNetwork(*wifi)),
      ssid_textfield_(NULL),
      identity_textfield_(NULL),
      certificate_browse_button_(NULL),
      certificate_path_(),
      security_combobox_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init();
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent)
    : parent_(parent),
      can_login_(false),
      ssid_textfield_(NULL),
      identity_textfield_(NULL),
      certificate_browse_button_(NULL),
      certificate_path_(),
      security_combobox_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init();
}

WifiConfigView::~WifiConfigView() {
}

void WifiConfigView::UpdateCanLogin(void) {
  static const size_t kMinWirelessPasswordLen = 5;

  bool can_login = true;
  if (!wifi_.get()) {
    // Enforce ssid is non empty.
    // If security is not none, also enforce passphrase is non empty.
    can_login = !GetSSID().empty() &&
        (security_combobox_->selected_item() == INDEX_NONE ||
            passphrase_textfield_->text().length() >= kMinWirelessPasswordLen);
  } else {
    // Connecting to an encrypted network
    if (passphrase_textfield_ != NULL) {
      // if the network requires a passphrase, make sure it is non empty.
      can_login &=
          passphrase_textfield_->text().length() >= kMinWirelessPasswordLen;
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

void WifiConfigView::UpdateErrorLabel(bool failed) {
  static const int kNoError = -1;
  int id = kNoError;
  if (wifi_.get()) {
    // Right now, only displaying bad_passphrase and bad_wepkey errors.
    if (wifi_->error() == ERROR_BAD_PASSPHRASE)
      id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE;
    else if (wifi_->error() == ERROR_BAD_WEPKEY)
      id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_WEPKEY;
  }
  if (id == kNoError && failed) {
    // We don't know what the error was. For now assume bad identity or
    // passphrase. See TODO comment in Login() and crosbug.com/9538.
    id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_CREDENTIALS;
  }
  if (id != kNoError) {
    error_label_->SetText(UTF16ToWide(l10n_util::GetStringUTF16(id)));
    error_label_->SetVisible(true);
  } else {
    error_label_->SetVisible(false);
  }
}

void WifiConfigView::ContentsChanged(views::Textfield* sender,
                                     const string16& new_contents) {
  UpdateCanLogin();
}

bool WifiConfigView::HandleKeyEvent(views::Textfield* sender,
                                    const views::KeyEvent& key_event) {
  if (sender == passphrase_textfield_ &&
      key_event.GetKeyCode() == ui::VKEY_RETURN) {
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

void WifiConfigView::ItemChanged(views::Combobox* combo_box,
                                 int prev_index, int new_index) {
  // If changed to no security, then disable combobox and clear it.
  // Otherwise, enable it. Also, update can login.
  if (new_index == INDEX_NONE) {
    passphrase_textfield_->SetEnabled(false);
    passphrase_textfield_->SetText(string16());
  } else {
    passphrase_textfield_->SetEnabled(true);
  }
  UpdateCanLogin();
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
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  bool connected = false;
  if (!wifi_.get()) {
    ConnectionSecurity sec = SECURITY_UNKNOWN;
    int index = security_combobox_->selected_item();
    if (index == INDEX_NONE)
      sec = SECURITY_NONE;
    else if (index == INDEX_WEP)
      sec = SECURITY_WEP;
    else if (index == INDEX_WPA)
      sec = SECURITY_WPA;
    else if (index == INDEX_RSN)
      sec = SECURITY_RSN;
    connected =  cros->ConnectToWifiNetwork(
        sec, GetSSID(), GetPassphrase(),
        identity_string, certificate_path_, true);
  } else {
    Save();
    connected = cros->ConnectToWifiNetwork(
        wifi_.get(), GetPassphrase(),
        identity_string, certificate_path_);
  }
  if (!connected) {
    // Assume this failed due to an invalid password.
    // TODO(stevenjb): Modify libcros to set an error code. Return 'false'
    // only on invalid password or other recoverable failure. crosbug.com/9538.
    // Update any error message and return false (keep dialog open).
    UpdateErrorLabel(true);
    return false;
  }
  return true;  // dialog will be closed
}

bool WifiConfigView::Save() {
  // Save password here.
  if (wifi_.get()) {
    bool changed = false;

    if (passphrase_textfield_) {
      std::string passphrase = UTF16ToUTF8(passphrase_textfield_->text());
      if (passphrase != wifi_->passphrase()) {
        wifi_->set_passphrase(passphrase);
        changed = true;
      }
    }

    if (changed)
      CrosLibrary::Get()->GetNetworkLibrary()->SaveWifiNetwork(wifi_.get());
  }
  return true;
}

void WifiConfigView::Cancel() {
  // If we have a bad passphrase error, clear the passphrase.
  if (wifi_.get() && (wifi_->error() == ERROR_BAD_PASSPHRASE ||
                      wifi_->error() == ERROR_BAD_WEPKEY)) {
    wifi_->set_passphrase(std::string());
    CrosLibrary::Get()->GetNetworkLibrary()->SaveWifiNetwork(wifi_.get());
  }
}

const std::string WifiConfigView::GetSSID() const {
  std::string result;
  if (ssid_textfield_ != NULL) {
    std::string untrimmed = UTF16ToUTF8(ssid_textfield_->text());
    TrimWhitespaceASCII(untrimmed, TRIM_ALL, &result);
  }
  return result;
}

const std::string WifiConfigView::GetPassphrase() const {
  std::string result;
  if (passphrase_textfield_ != NULL)
    result = UTF16ToUTF8(passphrase_textfield_->text());
  return result;
}

void WifiConfigView::Init() {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
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

  // SSID input
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID))));
  if (!wifi_.get()) {
    ssid_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    ssid_textfield_->SetController(this);
    layout->AddView(ssid_textfield_);
  } else {
    views::Label* label = new views::Label(ASCIIToWide(wifi_->name()));
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(label);
  }
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Certificate input
  // Loaded certificates (i.e. stored in a pkcs11 device) do not require
  // a passphrase.
  bool certificate_loaded = false;

  // Add ID and cert password if we're using 802.1x
  // XXX we're cheating and assuming 802.1x means EAP-TLS - not true
  // in general, but very common. WPA Supplicant doesn't report the
  // EAP type because it's unknown until the process begins, and we'd
  // need some kind of callback.
  if (wifi_.get() && wifi_->encrypted() &&
      wifi_->encryption() == SECURITY_8021X) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY))));
    identity_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_textfield_->SetController(this);
    if (!wifi_->identity().empty())
      identity_textfield_->SetText(UTF8ToUTF16(wifi_->identity()));
    layout->AddView(identity_textfield_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT))));
    if (!wifi_->cert_path().empty()) {
      certificate_path_ = wifi_->cert_path();
      certificate_loaded = wifi_->IsCertificateLoaded();
    }
    if (certificate_loaded) {
      std::wstring label = UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_INSTALLED));
      views::Label* cert_text = new views::Label(label);
      cert_text->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      layout->AddView(cert_text);
    } else {
      std::wstring label;
      if (!certificate_path_.empty())
        label = UTF8ToWide(certificate_path_);
      else
        label = UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_BUTTON));
      certificate_browse_button_ = new views::NativeButton(this, label);
      layout->AddView(certificate_browse_button_);
    }
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  // Security select
  if (!wifi_.get()) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY))));
    security_combobox_ = new views::Combobox(new SecurityComboboxModel());
    security_combobox_->set_listener(this);
    layout->AddView(security_combobox_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  // Passphrase input
  layout->StartRow(0, column_view_set_id);
  int label_text_id;
  if (wifi_.get() && wifi_->encryption() == SECURITY_8021X) {
    label_text_id =
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PRIVATE_KEY_PASSWORD;
  } else {
    label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE;
  }
  layout->AddView(new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(label_text_id))));
  passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_PASSWORD);
  passphrase_textfield_->SetController(this);
  if (wifi_.get() && !wifi_->passphrase().empty())
    passphrase_textfield_->SetText(UTF8ToUTF16(wifi_->passphrase()));
  // Disable passphrase input initially for other network.
  if (!wifi_.get())
    passphrase_textfield_->SetEnabled(false);
  layout->AddView(passphrase_textfield_);
  // Password visible button.
  passphrase_visible_button_ = new views::ImageButton(this);
  passphrase_visible_button_->SetImage(
      views::ImageButton::BS_NORMAL,
      ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
  passphrase_visible_button_->SetImageAlignment(
      views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
  layout->AddView(passphrase_visible_button_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Create an error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetColor(SK_ColorRED);
  layout->AddView(error_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  // Set or hide the error text.
  UpdateErrorLabel(false);
}

}  // namespace chromeos
