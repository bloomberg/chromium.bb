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
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

namespace chromeos {

namespace {

// The width of the password field.
const int kPasswordWidth = 150;

enum SecurityComboboxIndex {
  SECURITY_INDEX_NONE  = 0,
  SECURITY_INDEX_WEP   = 1,
  SECURITY_INDEX_WPA   = 2,
  SECURITY_INDEX_RSN   = 3,
  SECURITY_INDEX_COUNT = 4
};

class SecurityComboboxModel : public ui::ComboboxModel {
 public:
  SecurityComboboxModel() {}
  virtual ~SecurityComboboxModel() {}
  virtual int GetItemCount() {
    return SECURITY_INDEX_COUNT;
  }
  virtual string16 GetItemAt(int index) {
    if (index == SECURITY_INDEX_NONE)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_NONE);
    else if (index == SECURITY_INDEX_WEP)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_WEP);
    else if (index == SECURITY_INDEX_WPA)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_WPA);
    else if (index == SECURITY_INDEX_RSN)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_RSN);
    NOTREACHED();
    return string16();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityComboboxModel);
};

enum EAPMethodComboboxIndex {
  EAP_METHOD_INDEX_NONE  = 0,
  EAP_METHOD_INDEX_PEAP  = 1,
  EAP_METHOD_INDEX_TLS   = 2,
  EAP_METHOD_INDEX_TTLS  = 3,
  EAP_METHOD_INDEX_LEAP  = 4,
  EAP_METHOD_INDEX_COUNT = 5
};

class EAPMethodComboboxModel : public ui::ComboboxModel {
 public:
  EAPMethodComboboxModel() {}
  virtual ~EAPMethodComboboxModel() {}
  virtual int GetItemCount() {
    return EAP_METHOD_INDEX_COUNT;
  }
  virtual string16 GetItemAt(int index) {
    if (index == EAP_METHOD_INDEX_NONE)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_NONE);
    else if (index == EAP_METHOD_INDEX_PEAP)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_PEAP);
    else if (index == EAP_METHOD_INDEX_TLS)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_TLS);
    else if (index == EAP_METHOD_INDEX_TTLS)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_TTLS);
    else if (index == EAP_METHOD_INDEX_LEAP)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_LEAP);
    NOTREACHED();
    return string16();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(EAPMethodComboboxModel);
};

enum Phase2AuthComboboxIndex {
  PHASE_2_AUTH_INDEX_AUTO     = 0, // LEAP, EAP-TLS have only this auth.
  PHASE_2_AUTH_INDEX_MD5      = 1,
  PHASE_2_AUTH_INDEX_MSCHAPV2 = 2, // PEAP has up to this auth.
  PHASE_2_AUTH_INDEX_MSCHAP   = 3,
  PHASE_2_AUTH_INDEX_PAP      = 4,
  PHASE_2_AUTH_INDEX_CHAP     = 5, // EAP-TTLS has up to this auth.
  PHASE_2_AUTH_INDEX_COUNT    = 6
};

class Phase2AuthComboboxModel : public ui::ComboboxModel {
 public:
  explicit Phase2AuthComboboxModel(views::Combobox* eap_method_combobox)
      : eap_method_combobox_(eap_method_combobox) {}
  virtual ~Phase2AuthComboboxModel() {}
  virtual int GetItemCount() {
    switch (eap_method_combobox_->selected_item()) {
      case EAP_METHOD_INDEX_NONE:
      case EAP_METHOD_INDEX_TLS:
      case EAP_METHOD_INDEX_LEAP:
        return PHASE_2_AUTH_INDEX_AUTO + 1;
      case EAP_METHOD_INDEX_PEAP:
        return PHASE_2_AUTH_INDEX_MSCHAPV2 + 1;
      case EAP_METHOD_INDEX_TTLS:
        return PHASE_2_AUTH_INDEX_CHAP + 1;
    }
    NOTREACHED();
    return 0;
  }
  virtual string16 GetItemAt(int index) {
    if (index == PHASE_2_AUTH_INDEX_AUTO)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH_AUTO);
    else if (index == PHASE_2_AUTH_INDEX_MD5)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH_MD5);
    else if (index == PHASE_2_AUTH_INDEX_MSCHAPV2)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH_MSCHAPV2);
    else if (index == PHASE_2_AUTH_INDEX_MSCHAP)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH_MSCHAP);
    else if (index == PHASE_2_AUTH_INDEX_PAP)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH_PAP);
    else if (index == PHASE_2_AUTH_INDEX_CHAP)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH_CHAP);
    NOTREACHED();
    return string16();
  }
 private:
  views::Combobox* eap_method_combobox_;
  DISALLOW_COPY_AND_ASSIGN(Phase2AuthComboboxModel);
};

// TODO(chocobo): Added logic for getting server CA certs combobox populated.
class ServerCACertComboboxModel : public ui::ComboboxModel {
 public:
  ServerCACertComboboxModel() {}
  virtual ~ServerCACertComboboxModel() {}
  virtual int GetItemCount() {
    return 3;
  }
  virtual string16 GetItemAt(int index) {
    if (index == 0)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_USE_SYSTEM);
    if (index == GetItemCount()-1)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_DO_NOT_CHECK);
    return UTF8ToUTF16("/tmp/ca-cert.pem");
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ServerCACertComboboxModel);
};

// TODO(chocobo): Added logic for getting client certs combobox populated.
class ClientCertComboboxModel : public ui::ComboboxModel {
 public:
  ClientCertComboboxModel() {}
  virtual ~ClientCertComboboxModel() {}
  virtual int GetItemCount() {
    return 1;
  }
  virtual string16 GetItemAt(int index) {
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_NONE);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ClientCertComboboxModel);
};

}  // namespace

WifiConfigView::WifiConfigView(NetworkConfigView* parent, WifiNetwork* wifi)
    : parent_(parent),
      is_8021x_(false),
      service_path_(wifi->service_path()),
      ssid_textfield_(NULL),
      eap_method_combobox_(NULL),
      phase_2_auth_combobox_(NULL),
      client_cert_combobox_(NULL),
      server_ca_cert_combobox_(NULL),
      identity_textfield_(NULL),
      identity_anonymous_textfield_(NULL),
      certificate_browse_button_(NULL),
      certificate_path_(),
      security_combobox_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(wifi);
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent)
    : parent_(parent),
      is_8021x_(false),
      ssid_textfield_(NULL),
      eap_method_combobox_(NULL),
      phase_2_auth_combobox_(NULL),
      client_cert_combobox_(NULL),
      server_ca_cert_combobox_(NULL),
      identity_textfield_(NULL),
      identity_anonymous_textfield_(NULL),
      certificate_browse_button_(NULL),
      certificate_path_(),
      security_combobox_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(NULL);
}

WifiConfigView::~WifiConfigView() {
}

bool WifiConfigView::CanLogin() {
  static const size_t kMinWirelessPasswordLen = 5;

  if (service_path_.empty()) {
    // Enforce ssid is non empty.
    if (GetSSID().empty())
      return false;

    // If security is not none, also enforce passphrase is non empty.
    if (security_combobox_->selected_item() != SECURITY_INDEX_NONE &&
        passphrase_textfield_->text().length() < kMinWirelessPasswordLen)
      return false;
  } else {
    if (is_8021x_) {
      // Make sure the EAP method is set
      if (eap_method_combobox_->selected_item() == EAP_METHOD_INDEX_NONE)
        return false;
    } else {
      // if the network requires a passphrase, make sure it is the right length.
      if (passphrase_textfield_ != NULL &&
          passphrase_textfield_->text().length() < kMinWirelessPasswordLen)
        return false;
    }
  }
  return true;
}

void WifiConfigView::UpdateDialogButtons() {
  parent_->GetDialogClientView()->UpdateDialogButtons();
}

void WifiConfigView::RefreshEAPFields() {
  int selected = eap_method_combobox_->selected_item();

  // If EAP method changes, the phase 2 auth choices may have changed also.
  phase_2_auth_combobox_->ModelChanged();
  phase_2_auth_combobox_->SetSelectedItem(0);
  phase_2_auth_combobox_->SetEnabled(
      phase_2_auth_combobox_->model()->GetItemCount() > 1);

  // No password for EAP-TLS
  passphrase_textfield_->SetEnabled(selected != EAP_METHOD_INDEX_NONE &&
                                    selected != EAP_METHOD_INDEX_TLS);
  if (!passphrase_textfield_->IsEnabled())
    passphrase_textfield_->SetText(string16());

  // Client certs only for EAP-TLS
  client_cert_combobox_->SetEnabled(selected == EAP_METHOD_INDEX_TLS);

  // No server CA certs for LEAP
  server_ca_cert_combobox_->SetEnabled(selected != EAP_METHOD_INDEX_NONE &&
                                       selected != EAP_METHOD_INDEX_LEAP);

  // No anonymous identity if no phase 2 auth.
  identity_anonymous_textfield_->SetEnabled(
      phase_2_auth_combobox_->IsEnabled());
  if (!identity_anonymous_textfield_->IsEnabled())
    identity_anonymous_textfield_->SetText(string16());
}

void WifiConfigView::UpdateErrorLabel(bool failed) {
  static const int kNoError = -1;
  int id = kNoError;
  if (!service_path_.empty()) {
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    const WifiNetwork* wifi = cros->FindWifiNetworkByPath(service_path_);
    if (wifi) {
      // Right now, only displaying bad_passphrase and bad_wepkey errors.
      if (wifi->error() == ERROR_BAD_PASSPHRASE)
        id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE;
      else if (wifi->error() == ERROR_BAD_WEPKEY)
        id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_WEPKEY;
    }
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
  UpdateDialogButtons();
}

bool WifiConfigView::HandleKeyEvent(views::Textfield* sender,
                                    const views::KeyEvent& key_event) {
  if (sender == passphrase_textfield_ &&
      key_event.key_code() == ui::VKEY_RETURN) {
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
  if (new_index == prev_index)
    return;
  if (combo_box == security_combobox_) {
    // If changed to no security, then disable combobox and clear it.
    // Otherwise, enable it. Also, update can login.
    if (new_index == SECURITY_INDEX_NONE) {
      passphrase_textfield_->SetEnabled(false);
      passphrase_textfield_->SetText(string16());
    } else {
      passphrase_textfield_->SetEnabled(true);
    }
  } else if (combo_box == eap_method_combobox_) {
    RefreshEAPFields();
  }
  UpdateDialogButtons();
}

void WifiConfigView::FileSelected(const FilePath& path,
                                   int index, void* params) {
  certificate_path_ = path.value();
  if (certificate_browse_button_) {
    certificate_browse_button_->SetLabel(
        UTF16ToWide(path.BaseName().LossyDisplayName()));
  }
  UpdateDialogButtons();  // TODO(njw) Check if the passphrase decrypts the key.
}

bool WifiConfigView::Login() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (service_path_.empty()) {
    std::string identity_string;
    if (identity_textfield_ != NULL) {
      identity_string = UTF16ToUTF8(identity_textfield_->text());
    }
    ConnectionSecurity sec = SECURITY_UNKNOWN;
    switch (security_combobox_->selected_item()) {
      case SECURITY_INDEX_NONE:
        sec = SECURITY_NONE;
        break;
      case SECURITY_INDEX_WEP:
        sec = SECURITY_WEP;
        break;
      case SECURITY_INDEX_WPA:
        sec = SECURITY_WPA;
        break;
      case SECURITY_INDEX_RSN:
        sec = SECURITY_RSN;
        break;
    }
    cros->ConnectToWifiNetwork(
        sec, GetSSID(), GetPassphrase(),
        identity_string, certificate_path_, true);
  } else {
    WifiNetwork* wifi = cros->FindWifiNetworkByPath(service_path_);
    if (!wifi) {
      // Flimflam no longer knows about this wifi network (edge case).
      // TODO(stevenjb): Add a notification (chromium-os13225).
      LOG(WARNING) << "Wifi network: " << service_path_ << " no longer exists.";
      return true;
    }
    if (is_8021x_) {
      // EAP method
      EAPMethod method = EAP_METHOD_UNKNOWN;
      switch (eap_method_combobox_->selected_item()) {
        case EAP_METHOD_INDEX_PEAP:
          method = EAP_METHOD_PEAP;
          break;
        case EAP_METHOD_INDEX_TLS:
          method = EAP_METHOD_TLS;
          break;
        case EAP_METHOD_INDEX_TTLS:
          method = EAP_METHOD_TTLS;
          break;
        case EAP_METHOD_INDEX_LEAP:
          method = EAP_METHOD_LEAP;
          break;
      }
      DCHECK(method != EAP_METHOD_UNKNOWN);
      wifi->SetEAPMethod(method);

      // Phase 2 authentication
      if (phase_2_auth_combobox_->IsEnabled()) {
        EAPPhase2Auth auth = EAP_PHASE_2_AUTH_AUTO;
        switch (phase_2_auth_combobox_->selected_item()) {
          case PHASE_2_AUTH_INDEX_MD5:
            auth = EAP_PHASE_2_AUTH_MD5;
            break;
          case PHASE_2_AUTH_INDEX_MSCHAPV2:
            auth = EAP_PHASE_2_AUTH_MSCHAPV2;
            break;
          case PHASE_2_AUTH_INDEX_MSCHAP:
            auth = EAP_PHASE_2_AUTH_MSCHAP;
            break;
          case PHASE_2_AUTH_INDEX_PAP:
            auth = EAP_PHASE_2_AUTH_PAP;
            break;
          case PHASE_2_AUTH_INDEX_CHAP:
            auth = EAP_PHASE_2_AUTH_CHAP;
            break;
        }
        wifi->SetEAPPhase2Auth(auth);
      }

      // Server CA certificate
      if (server_ca_cert_combobox_->IsEnabled()) {
        int selected = server_ca_cert_combobox_->selected_item();
        if (selected == 0) {
          wifi->SetEAPServerCACert(std::string());
          wifi->SetEAPUseSystemCAs(true);
        } else if (selected ==
            server_ca_cert_combobox_->model()->GetItemCount()-1) {
          wifi->SetEAPServerCACert(std::string());
          wifi->SetEAPUseSystemCAs(false);
        } else {
          wifi->SetEAPServerCACert(UTF16ToUTF8(
              server_ca_cert_combobox_->model()->GetItemAt(selected)));
        }
      }

      // Client certificate
      if (client_cert_combobox_->IsEnabled()) {
        int selected = client_cert_combobox_->selected_item();
        if (selected == 0) {
          wifi->SetEAPClientCert(std::string());
        } else {
          wifi->SetEAPClientCert(
              UTF16ToUTF8(client_cert_combobox_->model()->GetItemAt(selected)));
        }
      }

      // Identity
      if (identity_textfield_->IsEnabled()) {
        wifi->SetEAPIdentity(UTF16ToUTF8(identity_textfield_->text()));
      }

      // Anonymous identity
      if (identity_anonymous_textfield_->IsEnabled()) {
        wifi->SetEAPAnonymousIdentity(
            UTF16ToUTF8(identity_anonymous_textfield_->text()));
      }

      // Passphrase
      if (passphrase_textfield_->IsEnabled()) {
        wifi->SetEAPPassphrase(UTF16ToUTF8(passphrase_textfield_->text()));
      }
    } else {
      const std::string passphrase = GetPassphrase();
      if (passphrase != wifi->passphrase())
        wifi->SetPassphrase(passphrase);
    }

    cros->ConnectToWifiNetwork(wifi);
    // Connection failures are responsible for updating the UI, including
    // reopening dialogs.
  }
  return true;  // dialog will be closed
}

void WifiConfigView::Cancel() {
}

std::string WifiConfigView::GetSSID() const {
  std::string result;
  if (ssid_textfield_ != NULL) {
    std::string untrimmed = UTF16ToUTF8(ssid_textfield_->text());
    TrimWhitespaceASCII(untrimmed, TRIM_ALL, &result);
  }
  return result;
}

std::string WifiConfigView::GetPassphrase() const {
  std::string result;
  if (passphrase_textfield_ != NULL)
    result = UTF16ToUTF8(passphrase_textfield_->text());
  return result;
}

// This will initialize the view depending on if we have a wifi network or not.
// And if we are doing simple password encyption or the more complicated
// 802.1x encryption.
// If we are creating the "Join other network..." dialog, we will allow user
// to enter the data. And if they select the 802.1x encryption, we will show
// the 802.1x fields.
void WifiConfigView::Init(WifiNetwork* wifi) {
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
  if (!wifi) {
    ssid_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    ssid_textfield_->SetController(this);
    ssid_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID));
    layout->AddView(ssid_textfield_);
  } else {
    views::Label* label = new views::Label(ASCIIToWide(wifi->name()));
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(label);
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Security select
  if (!wifi) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY))));
    security_combobox_ = new views::Combobox(new SecurityComboboxModel());
    security_combobox_->set_listener(this);
    layout->AddView(security_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  is_8021x_ = wifi && wifi->encrypted() &&
      wifi->encryption() == SECURITY_8021X;
  if (is_8021x_) {
    // EAP method
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD))));
    eap_method_combobox_ = new views::Combobox(new EAPMethodComboboxModel());
    eap_method_combobox_->set_listener(this);
    layout->AddView(eap_method_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Phase 2 authentication
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH))));
    phase_2_auth_combobox_ = new views::Combobox(
        new Phase2AuthComboboxModel(eap_method_combobox_));
    phase_2_auth_combobox_->SetEnabled(false);
    phase_2_auth_combobox_->set_listener(this);
    layout->AddView(phase_2_auth_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Server CA certificate
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA))));
    server_ca_cert_combobox_ = new views::Combobox(
        new ServerCACertComboboxModel());
    server_ca_cert_combobox_->SetEnabled(false);
    server_ca_cert_combobox_->set_listener(this);
    layout->AddView(server_ca_cert_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Client certificate
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT))));
    client_cert_combobox_ = new views::Combobox(
        new ClientCertComboboxModel());
    client_cert_combobox_->SetEnabled(false);
    client_cert_combobox_->set_listener(this);
    layout->AddView(client_cert_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Identity
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY))));
    identity_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_textfield_->SetController(this);
    if (!wifi->identity().empty())
      identity_textfield_->SetText(UTF8ToUTF16(wifi->identity()));
    layout->AddView(identity_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Anonymous identity
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY_ANONYMOUS))));
    identity_anonymous_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_anonymous_textfield_->SetEnabled(false);
    identity_anonymous_textfield_->SetController(this);
    layout->AddView(identity_anonymous_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Passphrase input
  layout->StartRow(0, column_view_set_id);
  int label_text_id;
  if (is_8021x_)
    label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PRIVATE_KEY_PASSWORD;
  else
    label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE;
  layout->AddView(new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(label_text_id))));
  passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_PASSWORD);
  passphrase_textfield_->SetController(this);
  if (wifi && !wifi->GetPassphrase().empty())
    passphrase_textfield_->SetText(UTF8ToUTF16(wifi->GetPassphrase()));
  // Disable passphrase input initially for other network.
  if (!wifi)
    passphrase_textfield_->SetEnabled(false);
  passphrase_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      label_text_id));
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
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // After creating the fields, we set the values. Fields need to be created
  // first because RefreshEAPFields() will enable/disable them as appropriate.
  if (is_8021x_) {
    // EAP Method
    switch (wifi->eap_method()) {
      case EAP_METHOD_PEAP:
        eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_PEAP);
        break;
      case EAP_METHOD_TLS:
        eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_TLS);
        break;
      case EAP_METHOD_TTLS:
        eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_TTLS);
        break;
      case EAP_METHOD_LEAP:
        eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_LEAP);
        break;
      default:
        break;
    }
    RefreshEAPFields();

    // Phase 2 authentication
    if (phase_2_auth_combobox_->IsEnabled()) {
      switch (wifi->eap_phase_2_auth()) {
        case EAP_PHASE_2_AUTH_MD5:
          phase_2_auth_combobox_->SetSelectedItem(PHASE_2_AUTH_INDEX_MD5);
          break;
        case EAP_PHASE_2_AUTH_MSCHAPV2:
          phase_2_auth_combobox_->SetSelectedItem(PHASE_2_AUTH_INDEX_MSCHAPV2);
          break;
        case EAP_PHASE_2_AUTH_MSCHAP:
          phase_2_auth_combobox_->SetSelectedItem(PHASE_2_AUTH_INDEX_MSCHAP);
          break;
        case EAP_PHASE_2_AUTH_PAP:
          phase_2_auth_combobox_->SetSelectedItem(PHASE_2_AUTH_INDEX_PAP);
          break;
        case EAP_PHASE_2_AUTH_CHAP:
          phase_2_auth_combobox_->SetSelectedItem(PHASE_2_AUTH_INDEX_CHAP);
          break;
        default:
          break;
      }
    }

    // Server CA certificate
    if (server_ca_cert_combobox_->IsEnabled()) {
      const std::string& cert = wifi->eap_server_ca_cert();
      if (cert.empty()) {
        if (wifi->eap_use_system_cas())
          server_ca_cert_combobox_->SetSelectedItem(0);
        else  // select last item for "Do Not Check"
          server_ca_cert_combobox_->SetSelectedItem(
              server_ca_cert_combobox_->model()->GetItemCount()-1);
      } else {
        // select the certificate path if available
        // Note: the last item is "Do Not Check" so no need to compare that.
        for (int i = 1;
             i < server_ca_cert_combobox_->model()->GetItemCount()-1;
             i++) {
          if (cert ==
              UTF16ToUTF8(server_ca_cert_combobox_->model()->GetItemAt(i))) {
            server_ca_cert_combobox_->SetSelectedItem(i);
            break;
          }
        }
      }
    }

    // Client certificate
    if (client_cert_combobox_->IsEnabled()) {
      const std::string& cert = wifi->eap_client_cert();
      if (cert.empty()) {
        client_cert_combobox_->SetSelectedItem(0);
      } else {
        // select the certificate path if available
        for (int i = 1;
             i < client_cert_combobox_->model()->GetItemCount();
             i++) {
          if (cert ==
              UTF16ToUTF8(client_cert_combobox_->model()->GetItemAt(i))) {
            client_cert_combobox_->SetSelectedItem(i);
            break;
          }
        }
      }
    }

    // Identity
    if (identity_textfield_->IsEnabled())
      identity_textfield_->SetText(UTF8ToUTF16(wifi->eap_identity()));

    // Anonymous identity
    if (identity_anonymous_textfield_->IsEnabled())
      identity_anonymous_textfield_->SetText(
          UTF8ToUTF16(wifi->eap_anonymous_identity()));

    // Passphrase
    if (passphrase_textfield_->IsEnabled())
      passphrase_textfield_->SetText(UTF8ToUTF16(wifi->eap_passphrase()));
  }

  // Create an error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetColor(SK_ColorRED);
  layout->AddView(error_label_);

  // Set or hide the error text.
  UpdateErrorLabel(false);
}

}  // namespace chromeos
