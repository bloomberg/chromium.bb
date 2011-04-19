// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_view.h"

#include "base/command_line.h"  // TODO(jamescook): Remove.
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/wifi_config_model.h"
#include "chrome/common/chrome_switches.h"  // TODO(jamescook): Remove.
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

namespace chromeos {

namespace {

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

// TODO(jamescook):  For M12 we only expose PEAP and EAP-TTLS.  Later, when
// when we support all methods by default, order this list to be alphabetical.
enum EAPMethodComboboxIndex {
  EAP_METHOD_INDEX_NONE  = 0,
  EAP_METHOD_INDEX_PEAP  = 1,
  EAP_METHOD_INDEX_TTLS  = 2,  // By default we support up to here.
  EAP_METHOD_INDEX_TLS   = 3,
  EAP_METHOD_INDEX_LEAP  = 4,  // Flag "--enable-all-eap" allows up to here.
  EAP_METHOD_INDEX_COUNT = 5
};

class EAPMethodComboboxModel : public ui::ComboboxModel {
 public:
  EAPMethodComboboxModel() {}
  virtual ~EAPMethodComboboxModel() {}
  virtual int GetItemCount() {
    // TODO(jamescook):  For M12 we only expose PEAP and EAP-TTLS by default.
    // Remove this switch when all methods are supported.
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExperimentalEap))
      return EAP_METHOD_INDEX_TTLS + 1;
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

// Combobox that supports a preferred width.  Used by Server CA combobox
// because the strings inside it are too wide.
class ComboboxWithWidth : public views::Combobox {
 public:
  ComboboxWithWidth(ui::ComboboxModel* model, int width)
      : Combobox(model),
        width_(width) {
  }
  virtual ~ComboboxWithWidth() {}
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = Combobox::GetPreferredSize();
    size.set_width(width_);
    return size;
  }
 private:
  int width_;
  DISALLOW_COPY_AND_ASSIGN(ComboboxWithWidth);
};

class ServerCACertComboboxModel : public ui::ComboboxModel {
 public:
  explicit ServerCACertComboboxModel(WifiConfigModel* wifi_config_model)
      : wifi_config_model_(wifi_config_model) {
  }
  virtual ~ServerCACertComboboxModel() {}
  virtual int GetItemCount() {
    // First "Default", then the certs, then "Do not check".
    return wifi_config_model_->GetServerCaCertCount() + 2;
  }
  virtual string16 GetItemAt(int combo_index) {
    if (combo_index == 0)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA_DEFAULT);
    if (combo_index == GetItemCount() - 1)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA_DO_NOT_CHECK);
    int cert_index = combo_index - 1;
    return wifi_config_model_->GetServerCaCertName(cert_index);
  }
 private:
  WifiConfigModel* wifi_config_model_;
  DISALLOW_COPY_AND_ASSIGN(ServerCACertComboboxModel);
};

class ClientCertComboboxModel : public ui::ComboboxModel {
 public:
  explicit ClientCertComboboxModel(WifiConfigModel* wifi_config_model)
      : wifi_config_model_(wifi_config_model) {
  }
  virtual ~ClientCertComboboxModel() {}
  virtual int GetItemCount() {
    // One initial item "None", then the certs.
    return 1 + wifi_config_model_->GetUserCertCount();
  }
  virtual string16 GetItemAt(int combo_index) {
    if (combo_index == 0)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_NONE);
    int cert_index = combo_index - 1;
    return wifi_config_model_->GetUserCertName(cert_index);
  }
 private:
  WifiConfigModel* wifi_config_model_;
  DISALLOW_COPY_AND_ASSIGN(ClientCertComboboxModel);
};

}  // namespace

WifiConfigView::WifiConfigView(NetworkConfigView* parent, WifiNetwork* wifi)
    : ChildNetworkConfigView(parent, wifi),
      wifi_config_model_(new WifiConfigModel()),
      is_8021x_(false),
      ssid_textfield_(NULL),
      eap_method_combobox_(NULL),
      phase_2_auth_label_(NULL),
      phase_2_auth_combobox_(NULL),
      client_cert_label_(NULL),
      client_cert_combobox_(NULL),
      server_ca_cert_label_(NULL),
      server_ca_cert_combobox_(NULL),
      identity_label_(NULL),
      identity_textfield_(NULL),
      identity_anonymous_label_(NULL),
      identity_anonymous_textfield_(NULL),
      save_credentials_checkbox_(NULL),
      security_combobox_(NULL),
      passphrase_label_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(wifi);
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent)
    : ChildNetworkConfigView(parent),
      wifi_config_model_(new WifiConfigModel()),
      is_8021x_(false),
      ssid_textfield_(NULL),
      eap_method_combobox_(NULL),
      phase_2_auth_label_(NULL),
      phase_2_auth_combobox_(NULL),
      client_cert_label_(NULL),
      client_cert_combobox_(NULL),
      server_ca_cert_label_(NULL),
      server_ca_cert_combobox_(NULL),
      identity_label_(NULL),
      identity_textfield_(NULL),
      identity_anonymous_label_(NULL),
      identity_anonymous_textfield_(NULL),
      save_credentials_checkbox_(NULL),
      security_combobox_(NULL),
      passphrase_label_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(NULL);
}

WifiConfigView::~WifiConfigView() {
}

string16 WifiConfigView::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_JOIN_WIFI_NETWORKS);
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
  phase_2_auth_label_->SetEnabled(phase_2_auth_combobox_->IsEnabled());

  // No password for EAP-TLS
  passphrase_textfield_->SetEnabled(selected != EAP_METHOD_INDEX_NONE &&
                                    selected != EAP_METHOD_INDEX_TLS);
  passphrase_label_->SetEnabled(passphrase_textfield_->IsEnabled());
  if (!passphrase_textfield_->IsEnabled())
    passphrase_textfield_->SetText(string16());

  // Client certs only for EAP-TLS
  if (client_cert_combobox_) {
    client_cert_combobox_->SetEnabled(selected == EAP_METHOD_INDEX_TLS);
    client_cert_label_->SetEnabled(client_cert_combobox_->IsEnabled());
  }

  // No server CA certs for LEAP
  server_ca_cert_combobox_->SetEnabled(selected != EAP_METHOD_INDEX_NONE &&
                                       selected != EAP_METHOD_INDEX_LEAP);
  server_ca_cert_label_->SetEnabled(server_ca_cert_combobox_->IsEnabled());

  // No anonymous identity if no phase 2 auth.
  identity_anonymous_textfield_->SetEnabled(
      phase_2_auth_combobox_->IsEnabled());
  identity_anonymous_label_->SetEnabled(
      identity_anonymous_textfield_->IsEnabled());
  if (!identity_anonymous_textfield_->IsEnabled())
    identity_anonymous_textfield_->SetText(string16());
}

void WifiConfigView::UpdateErrorLabel() {
  std::string error_msg;
  if (!service_path_.empty()) {
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    const WifiNetwork* wifi = cros->FindWifiNetworkByPath(service_path_);
    if (wifi && wifi->failed()) {
      bool passphrase_empty = wifi->GetPassphrase().empty();
      switch (wifi->error()) {
        case ERROR_BAD_PASSPHRASE:
          if (!passphrase_empty) {
            error_msg = l10n_util::GetStringUTF8(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE);
          }
          break;
        case ERROR_BAD_WEPKEY:
          if (!passphrase_empty) {
            error_msg = l10n_util::GetStringUTF8(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_WEPKEY);
          }
          break;
        default:
          error_msg = wifi->GetErrorString();
          break;
      }
    }
  }
  if (!error_msg.empty()) {
    error_label_->SetText(UTF8ToWide(error_msg));
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
      passphrase_label_->SetEnabled(false);
      passphrase_textfield_->SetEnabled(false);
      passphrase_textfield_->SetText(string16());
    } else {
      passphrase_label_->SetEnabled(true);
      passphrase_textfield_->SetEnabled(true);
    }
  } else if (combo_box == eap_method_combobox_) {
    RefreshEAPFields();
  }
  UpdateDialogButtons();
}

bool WifiConfigView::Login() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (service_path_.empty()) {
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
        sec, GetSSID(), GetPassphrase(), std::string(), std::string());
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
          // First item is "Default".
          wifi->SetEAPServerCaCertNssNickname(std::string());
          wifi->SetEAPUseSystemCAs(true);
        } else if (selected ==
            server_ca_cert_combobox_->model()->GetItemCount() - 1) {
          // Last item is "Do not check".
          wifi->SetEAPServerCaCertNssNickname(std::string());
          wifi->SetEAPUseSystemCAs(false);
        } else {
          int cert_index = selected - 1;
          std::string nss_nickname =
              wifi_config_model_->GetServerCaCertNssNickname(cert_index);
          wifi->SetEAPServerCaCertNssNickname(nss_nickname);
        }
      }

      // Client certificate
      if (client_cert_combobox_ && client_cert_combobox_->IsEnabled()) {
        int selected = client_cert_combobox_->selected_item();
        if (selected == 0) {
          // First item is "None".
          wifi->SetEAPClientCertPkcs11Id(std::string());
        } else {
          // Send cert ID to flimflam.
          int cert_index = selected - 1;
          std::string cert_pkcs11_id =
              wifi_config_model_->GetUserCertPkcs11Id(cert_index);
          wifi->SetEAPClientCertPkcs11Id(cert_pkcs11_id);
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

      // Save credentials
      wifi->SetSaveCredentials(save_credentials_checkbox_->checked());
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
                        views::GridLayout::USE_PREF, 0,
                        ChildNetworkConfigView::kPassphraseWidth);
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
    // Only enumerate certificates in the data model for 802.1X networks.
    wifi_config_model_->UpdateCertificates();

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
    phase_2_auth_label_ =
        new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH)));
    layout->AddView(phase_2_auth_label_);
    phase_2_auth_combobox_ = new views::Combobox(
        new Phase2AuthComboboxModel(eap_method_combobox_));
    phase_2_auth_label_->SetEnabled(false);
    phase_2_auth_combobox_->SetEnabled(false);
    phase_2_auth_combobox_->set_listener(this);
    layout->AddView(phase_2_auth_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Server CA certificate
    layout->StartRow(0, column_view_set_id);
    server_ca_cert_label_ =
        new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA)));
    layout->AddView(server_ca_cert_label_);
    server_ca_cert_combobox_ = new ComboboxWithWidth(
        new ServerCACertComboboxModel(wifi_config_model_.get()),
        ChildNetworkConfigView::kPassphraseWidth);
    server_ca_cert_label_->SetEnabled(false);
    server_ca_cert_combobox_->SetEnabled(false);
    server_ca_cert_combobox_->set_listener(this);
    layout->AddView(server_ca_cert_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // TODO(jamescook): Add back client certificate combobox when we support
    // EAP-TLS by default.
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExperimentalEap)) {
      // Client certificate
      layout->StartRow(0, column_view_set_id);
      client_cert_label_ = new views::Label(
          UTF16ToWide(l10n_util::GetStringUTF16(
              IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT)));
      layout->AddView(client_cert_label_);
      client_cert_combobox_ = new views::Combobox(
          new ClientCertComboboxModel(wifi_config_model_.get()));
      client_cert_label_->SetEnabled(false);
      client_cert_combobox_->SetEnabled(false);
      client_cert_combobox_->set_listener(this);
      layout->AddView(client_cert_combobox_);
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    }

    // Identity
    layout->StartRow(0, column_view_set_id);
    identity_label_ = new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY)));
    layout->AddView(identity_label_);
    identity_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_textfield_->SetController(this);
    if (!wifi->identity().empty())
      identity_textfield_->SetText(UTF8ToUTF16(wifi->identity()));
    layout->AddView(identity_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Passphrase input
  layout->StartRow(0, column_view_set_id);
  int label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE;
  passphrase_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(label_text_id)));
  layout->AddView(passphrase_label_);
  passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_PASSWORD);
  passphrase_textfield_->SetController(this);
  if (wifi && !wifi->GetPassphrase().empty())
    passphrase_textfield_->SetText(UTF8ToUTF16(wifi->GetPassphrase()));
  // Disable passphrase input initially for other network.
  if (!wifi) {
    passphrase_label_->SetEnabled(false);
    passphrase_textfield_->SetEnabled(false);
  }
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

  if (is_8021x_) {
    // Anonymous identity
    layout->StartRow(0, column_view_set_id);
    identity_anonymous_label_ =
        new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY_ANONYMOUS)));
    layout->AddView(identity_anonymous_label_);
    identity_anonymous_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_anonymous_label_->SetEnabled(false);
    identity_anonymous_textfield_->SetEnabled(false);
    identity_anonymous_textfield_->SetController(this);
    layout->AddView(identity_anonymous_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Save credentials
    layout->StartRow(0, column_view_set_id);
    save_credentials_checkbox_ = new views::Checkbox(
        UTF16ToWide(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SAVE_CREDENTIALS)));
    layout->SkipColumns(1);
    layout->AddView(save_credentials_checkbox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // After creating the fields, we set the values. Fields need to be created
  // first because RefreshEAPFields() will enable/disable them as appropriate.
  if (is_8021x_) {
    // EAP Method
    switch (wifi->eap_method()) {
      case EAP_METHOD_PEAP:
        eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_PEAP);
        break;
      case EAP_METHOD_TTLS:
        eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_TTLS);
        break;
      case EAP_METHOD_TLS:
        if (CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kEnableExperimentalEap))
          eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_TLS);
        else // Clean up from previous run with the switch set.
          eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_NONE);
        break;
      case EAP_METHOD_LEAP:
        if (CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kEnableExperimentalEap))
          eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_LEAP);
        else // Clean up from previous run with the switch set.
          eap_method_combobox_->SetSelectedItem(EAP_METHOD_INDEX_NONE);
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
      const std::string& nss_nickname = wifi->eap_server_ca_cert_nss_nickname();
      if (nss_nickname.empty()) {
        if (wifi->eap_use_system_cas()) {
          // "Default"
          server_ca_cert_combobox_->SetSelectedItem(0);
        } else {
          // "Do not check"
          server_ca_cert_combobox_->SetSelectedItem(
              server_ca_cert_combobox_->model()->GetItemCount() - 1);
        }
      } else {
        // select the certificate if available
        int cert_index = wifi_config_model_->GetServerCaCertIndex(nss_nickname);
        if (cert_index >= 0) {
          // Skip item for "Default"
          server_ca_cert_combobox_->SetSelectedItem(1 + cert_index);
        }
      }
    }

    // Client certificate
    if (client_cert_combobox_ && client_cert_combobox_->IsEnabled()) {
      const std::string& pkcs11_id = wifi->eap_client_cert_pkcs11_id();
      if (pkcs11_id.empty()) {
        // First item is "None".
        client_cert_combobox_->SetSelectedItem(0);
      } else {
        int cert_index = wifi_config_model_->GetUserCertIndex(pkcs11_id);
        if (cert_index >= 0) {
          // Skip item for "None"
          client_cert_combobox_->SetSelectedItem(1 + cert_index);
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

    // Save credentials
    bool save_credentials = (wifi ? wifi->save_credentials() : false);
    save_credentials_checkbox_->SetChecked(save_credentials);
  }

  // Create an error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetColor(SK_ColorRED);
  layout->AddView(error_label_);

  // Set or hide the error text.
  UpdateErrorLabel();
}

void WifiConfigView::InitFocus() {
  // Set focus to a reasonable widget, depending on what we're showing.
  if (ssid_textfield_)
    ssid_textfield_->RequestFocus();
  else if (eap_method_combobox_)
    eap_method_combobox_->RequestFocus();
  else if (passphrase_textfield_ && passphrase_textfield_->IsEnabled())
    passphrase_textfield_->RequestFocus();
}

}  // namespace chromeos
