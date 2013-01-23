// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_view.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/network/onc/onc_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace chromeos {

namespace {

// Returns true if network is known to require 802.1x.
bool Is8021x(const WifiNetwork* wifi) {
  return wifi && wifi->encrypted() && wifi->encryption() == SECURITY_8021X;
}

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

enum SecurityComboboxIndex {
  SECURITY_INDEX_NONE  = 0,
  SECURITY_INDEX_WEP   = 1,
  SECURITY_INDEX_PSK   = 2,
  SECURITY_INDEX_COUNT = 3
};

// Methods in alphabetical order.
enum EAPMethodComboboxIndex {
  EAP_METHOD_INDEX_NONE  = 0,
  EAP_METHOD_INDEX_LEAP  = 1,
  EAP_METHOD_INDEX_PEAP  = 2,
  EAP_METHOD_INDEX_TLS   = 3,
  EAP_METHOD_INDEX_TTLS  = 4,
  EAP_METHOD_INDEX_COUNT = 5
};

enum Phase2AuthComboboxIndex {
  PHASE_2_AUTH_INDEX_AUTO     = 0,  // LEAP, EAP-TLS have only this auth.
  PHASE_2_AUTH_INDEX_MD5      = 1,
  PHASE_2_AUTH_INDEX_MSCHAPV2 = 2,  // PEAP has up to this auth.
  PHASE_2_AUTH_INDEX_MSCHAP   = 3,
  PHASE_2_AUTH_INDEX_PAP      = 4,
  PHASE_2_AUTH_INDEX_CHAP     = 5,  // EAP-TTLS has up to this auth.
  PHASE_2_AUTH_INDEX_COUNT    = 6
};

}  // namespace

namespace internal {

class SecurityComboboxModel : public ui::ComboboxModel {
 public:
  SecurityComboboxModel();
  virtual ~SecurityComboboxModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityComboboxModel);
};

class EAPMethodComboboxModel : public ui::ComboboxModel {
 public:
  EAPMethodComboboxModel();
  virtual ~EAPMethodComboboxModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(EAPMethodComboboxModel);
};

class Phase2AuthComboboxModel : public ui::ComboboxModel {
 public:
  explicit Phase2AuthComboboxModel(views::Combobox* eap_method_combobox);
  virtual ~Phase2AuthComboboxModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  views::Combobox* eap_method_combobox_;

  DISALLOW_COPY_AND_ASSIGN(Phase2AuthComboboxModel);
};

class ServerCACertComboboxModel : public ui::ComboboxModel {
 public:
  explicit ServerCACertComboboxModel(CertLibrary* cert_library);
  virtual ~ServerCACertComboboxModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  CertLibrary* cert_library_;
  DISALLOW_COPY_AND_ASSIGN(ServerCACertComboboxModel);
};

class UserCertComboboxModel : public ui::ComboboxModel {
 public:
  explicit UserCertComboboxModel(CertLibrary* cert_library);
  virtual ~UserCertComboboxModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  CertLibrary* cert_library_;

  DISALLOW_COPY_AND_ASSIGN(UserCertComboboxModel);
};

// SecurityComboboxModel -------------------------------------------------------

SecurityComboboxModel::SecurityComboboxModel() {
}

SecurityComboboxModel::~SecurityComboboxModel() {
}

int SecurityComboboxModel::GetItemCount() const {
    return SECURITY_INDEX_COUNT;
  }
string16 SecurityComboboxModel::GetItemAt(int index) {
  if (index == SECURITY_INDEX_NONE)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_NONE);
  else if (index == SECURITY_INDEX_WEP)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_WEP);
  else if (index == SECURITY_INDEX_PSK)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY_PSK);
  NOTREACHED();
  return string16();
}

// EAPMethodComboboxModel ------------------------------------------------------

EAPMethodComboboxModel::EAPMethodComboboxModel() {
}

EAPMethodComboboxModel::~EAPMethodComboboxModel() {
}

int EAPMethodComboboxModel::GetItemCount() const {
  return EAP_METHOD_INDEX_COUNT;
}
string16 EAPMethodComboboxModel::GetItemAt(int index) {
  if (index == EAP_METHOD_INDEX_NONE)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_NONE);
  else if (index == EAP_METHOD_INDEX_LEAP)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_LEAP);
  else if (index == EAP_METHOD_INDEX_PEAP)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_PEAP);
  else if (index == EAP_METHOD_INDEX_TLS)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_TLS);
  else if (index == EAP_METHOD_INDEX_TTLS)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD_TTLS);
  NOTREACHED();
  return string16();
}

// Phase2AuthComboboxModel -----------------------------------------------------

Phase2AuthComboboxModel::Phase2AuthComboboxModel(
    views::Combobox* eap_method_combobox)
    : eap_method_combobox_(eap_method_combobox) {
}

Phase2AuthComboboxModel::~Phase2AuthComboboxModel() {
}

int Phase2AuthComboboxModel::GetItemCount() const {
  switch (eap_method_combobox_->selected_index()) {
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

string16 Phase2AuthComboboxModel::GetItemAt(int index) {
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

// ServerCACertComboboxModel ---------------------------------------------------

ServerCACertComboboxModel::ServerCACertComboboxModel(CertLibrary* cert_library)
    : cert_library_(cert_library) {
  DCHECK(cert_library);
}

ServerCACertComboboxModel::~ServerCACertComboboxModel() {
}

int ServerCACertComboboxModel::GetItemCount() const {
  if (cert_library_->CertificatesLoading())
    return 1;  // "Loading"
  // First "Default", then the certs, then "Do not check".
  return cert_library_->GetCACertificates().Size() + 2;
}

string16 ServerCACertComboboxModel::GetItemAt(int index) {
  if (cert_library_->CertificatesLoading())
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_LOADING);
  if (index == 0)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA_DEFAULT);
  if (index == GetItemCount() - 1)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA_DO_NOT_CHECK);
  int cert_index = index - 1;
  return cert_library_->GetCACertificates().GetDisplayStringAt(cert_index);
}

// UserCertComboboxModel -------------------------------------------------------

UserCertComboboxModel::UserCertComboboxModel(CertLibrary* cert_library)
    : cert_library_(cert_library) {
  DCHECK(cert_library);
}

UserCertComboboxModel::~UserCertComboboxModel() {
}

int UserCertComboboxModel::GetItemCount() const {
  if (cert_library_->CertificatesLoading())
    return 1;  // "Loading"
  int num_certs = cert_library_->GetUserCertificates().Size();
  if (num_certs == 0)
    return 1;  // "None installed"
  return num_certs;
}

string16 UserCertComboboxModel::GetItemAt(int index) {
  if (cert_library_->CertificatesLoading())
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_LOADING);
  if (cert_library_->GetUserCertificates().Size() == 0)
    return l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USER_CERT_NONE_INSTALLED);
  return cert_library_->GetUserCertificates().GetDisplayStringAt(index);
}

}  // namespace internal

WifiConfigView::WifiConfigView(NetworkConfigView* parent, WifiNetwork* wifi)
    : ChildNetworkConfigView(parent, wifi),
      cert_library_(NULL),
      ssid_textfield_(NULL),
      eap_method_combobox_(NULL),
      phase_2_auth_label_(NULL),
      phase_2_auth_combobox_(NULL),
      user_cert_label_(NULL),
      user_cert_combobox_(NULL),
      server_ca_cert_label_(NULL),
      server_ca_cert_combobox_(NULL),
      identity_label_(NULL),
      identity_textfield_(NULL),
      identity_anonymous_label_(NULL),
      identity_anonymous_textfield_(NULL),
      save_credentials_checkbox_(NULL),
      share_network_checkbox_(NULL),
      shared_network_label_(NULL),
      security_combobox_(NULL),
      passphrase_label_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(wifi, Is8021x(wifi));
}

WifiConfigView::WifiConfigView(NetworkConfigView* parent, bool show_8021x)
    : ChildNetworkConfigView(parent),
      cert_library_(NULL),
      ssid_textfield_(NULL),
      eap_method_combobox_(NULL),
      phase_2_auth_label_(NULL),
      phase_2_auth_combobox_(NULL),
      user_cert_label_(NULL),
      user_cert_combobox_(NULL),
      server_ca_cert_label_(NULL),
      server_ca_cert_combobox_(NULL),
      identity_label_(NULL),
      identity_textfield_(NULL),
      identity_anonymous_label_(NULL),
      identity_anonymous_textfield_(NULL),
      save_credentials_checkbox_(NULL),
      share_network_checkbox_(NULL),
      shared_network_label_(NULL),
      security_combobox_(NULL),
      passphrase_label_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(NULL, show_8021x);
}

WifiConfigView::~WifiConfigView() {
  if (cert_library_)
    cert_library_->RemoveObserver(this);
}

views::View* WifiConfigView::GetInitiallyFocusedView() {
  // Return a reasonable widget for initial focus,
  // depending on what we're showing.
  if (ssid_textfield_)
    return ssid_textfield_;
  else if (eap_method_combobox_)
    return eap_method_combobox_;
  else if (passphrase_textfield_ && passphrase_textfield_->enabled())
    return passphrase_textfield_;
  else
    return NULL;
}

bool WifiConfigView::CanLogin() {
  static const size_t kMinWirelessPasswordLen = 5;

  // We either have an existing wifi network or the user entered an SSID.
  if (service_path_.empty() && GetSsid().empty())
    return false;

  // If the network requires a passphrase, make sure it is the right length.
  if (passphrase_textfield_ != NULL
      && passphrase_textfield_->enabled()
      && passphrase_textfield_->text().length() < kMinWirelessPasswordLen)
    return false;

  // If we're using EAP, we must have a method.
  if (eap_method_combobox_ &&
      eap_method_combobox_->selected_index() == EAP_METHOD_INDEX_NONE)
    return false;

  // Block login if certs are required but user has none.
  if (UserCertRequired() && (!HaveUserCerts() || !IsUserCertValid()))
      return false;

  return true;
}

bool WifiConfigView::UserCertRequired() const {
  if (!cert_library_)
    return false;  // return false until cert_library_ is initialized.
  return UserCertActive();
}

bool WifiConfigView::HaveUserCerts() const {
  return cert_library_->GetUserCertificates().Size() > 0;
}

bool WifiConfigView::IsUserCertValid() const {
  if (!UserCertActive())
    return false;
  int index = user_cert_combobox_->selected_index();
  if (index < 0)
    return false;
  // Currently only hardware-backed user certificates are valid.
  if (cert_library_->IsHardwareBacked() &&
      !cert_library_->GetUserCertificates().IsHardwareBackedAt(index))
    return false;
  return true;
}

bool WifiConfigView::Phase2AuthActive() const {
  if (phase_2_auth_combobox_)
    return phase_2_auth_combobox_->model()->GetItemCount() > 1;
  return false;
}

bool WifiConfigView::PassphraseActive() const {
  if (eap_method_combobox_) {
    // No password for EAP-TLS.
    int index = eap_method_combobox_->selected_index();
    return index != EAP_METHOD_INDEX_NONE && index != EAP_METHOD_INDEX_TLS;
  } else if (security_combobox_) {
    return security_combobox_->selected_index() != SECURITY_INDEX_NONE;
  }
  return false;
}

bool WifiConfigView::UserCertActive() const {
  // User certs only for EAP-TLS.
  if (eap_method_combobox_)
    return eap_method_combobox_->selected_index() == EAP_METHOD_INDEX_TLS;

  return false;
}

bool WifiConfigView::CaCertActive() const {
  // No server CA certs for LEAP.
  if (eap_method_combobox_) {
    int index = eap_method_combobox_->selected_index();
    return index != EAP_METHOD_INDEX_NONE && index != EAP_METHOD_INDEX_LEAP;
  }
  return false;
}

void WifiConfigView::UpdateDialogButtons() {
  parent_->GetDialogClientView()->UpdateDialogButtons();
}

void WifiConfigView::RefreshEapFields() {
  DCHECK(cert_library_);

  // If EAP method changes, the phase 2 auth choices may have changed also.
  phase_2_auth_combobox_->ModelChanged();
  phase_2_auth_combobox_->SetSelectedIndex(0);
  bool phase_2_auth_enabled = Phase2AuthActive();
  phase_2_auth_combobox_->SetEnabled(phase_2_auth_enabled &&
                                     phase_2_auth_ui_data_.editable());
  phase_2_auth_label_->SetEnabled(phase_2_auth_enabled);

  // Passphrase.
  bool passphrase_enabled = PassphraseActive();
  passphrase_textfield_->SetEnabled(passphrase_enabled &&
                                    passphrase_ui_data_.editable());
  passphrase_label_->SetEnabled(passphrase_enabled);
  if (!passphrase_enabled)
    passphrase_textfield_->SetText(string16());

  // User cert.
  bool certs_loading = cert_library_->CertificatesLoading();
  bool user_cert_enabled = UserCertActive();
  user_cert_label_->SetEnabled(user_cert_enabled);
  bool have_user_certs = !certs_loading && HaveUserCerts();
  user_cert_combobox_->SetEnabled(user_cert_enabled &&
                                  have_user_certs &&
                                  user_cert_ui_data_.editable());
  user_cert_combobox_->ModelChanged();
  user_cert_combobox_->SetSelectedIndex(0);

  // Server CA.
  bool ca_cert_enabled = CaCertActive();
  server_ca_cert_label_->SetEnabled(ca_cert_enabled);
  server_ca_cert_combobox_->SetEnabled(ca_cert_enabled &&
                                       !certs_loading &&
                                       server_ca_cert_ui_data_.editable());
  server_ca_cert_combobox_->ModelChanged();
  server_ca_cert_combobox_->SetSelectedIndex(0);

  // No anonymous identity if no phase 2 auth.
  bool identity_anonymous_enabled = phase_2_auth_enabled;
  identity_anonymous_textfield_->SetEnabled(
      identity_anonymous_enabled && identity_anonymous_ui_data_.editable());
  identity_anonymous_label_->SetEnabled(identity_anonymous_enabled);
  if (!identity_anonymous_enabled)
    identity_anonymous_textfield_->SetText(string16());

  RefreshShareCheckbox();
}

void WifiConfigView::RefreshShareCheckbox() {
  if (!share_network_checkbox_)
    return;

  if (security_combobox_ &&
      security_combobox_->selected_index() == SECURITY_INDEX_NONE) {
    share_network_checkbox_->SetEnabled(false);
    share_network_checkbox_->SetChecked(true);
  } else if (eap_method_combobox_ &&
             (eap_method_combobox_->selected_index() == EAP_METHOD_INDEX_TLS ||
              user_cert_combobox_->selected_index() != 0)) {
    // Can not share TLS network (requires certificate), or any network where
    // user certificates are enabled.
    share_network_checkbox_->SetEnabled(false);
    share_network_checkbox_->SetChecked(false);
  } else if (!UserManager::Get()->IsUserLoggedIn()) {
    // If not logged in, networks must be shared.
    share_network_checkbox_->SetEnabled(false);
    share_network_checkbox_->SetChecked(true);
  } else {
    share_network_checkbox_->SetEnabled(true);
    share_network_checkbox_->SetChecked(false);  // Default to unshared.
  }
}

void WifiConfigView::UpdateErrorLabel() {
  std::string error_msg;
  if (UserCertRequired() && cert_library_->CertificatesLoaded()) {
    if (!HaveUserCerts()) {
      if (!UserManager::Get()->IsUserLoggedIn()) {
        error_msg = l10n_util::GetStringUTF8(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_LOGIN_FOR_USER_CERT);
      } else {
        error_msg = l10n_util::GetStringUTF8(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PLEASE_INSTALL_USER_CERT);
      }
    } else if (!IsUserCertValid()) {
      error_msg = l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_REQUIRE_HARDWARE_BACKED);
    }
  }
  if (error_msg.empty() && !service_path_.empty()) {
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
    error_label_->SetText(UTF8ToUTF16(error_msg));
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
                                    const ui::KeyEvent& key_event) {
  if (sender == passphrase_textfield_ &&
      key_event.key_code() == ui::VKEY_RETURN) {
    parent_->GetDialogClientView()->AcceptWindow();
  }
  return false;
}

void WifiConfigView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender == passphrase_visible_button_) {
    if (passphrase_textfield_) {
      passphrase_textfield_->SetObscured(!passphrase_textfield_->IsObscured());
      passphrase_visible_button_->SetToggled(
          !passphrase_textfield_->IsObscured());
    }
  } else {
    NOTREACHED();
  }
}

void WifiConfigView::OnSelectedIndexChanged(views::Combobox* combobox) {
  if (combobox == security_combobox_) {
    bool passphrase_enabled = PassphraseActive();
    passphrase_label_->SetEnabled(passphrase_enabled);
    passphrase_textfield_->SetEnabled(passphrase_enabled &&
                                      passphrase_ui_data_.editable());
    if (!passphrase_enabled)
      passphrase_textfield_->SetText(string16());
    RefreshShareCheckbox();
  } else if (combobox == user_cert_combobox_) {
    RefreshShareCheckbox();
  } else if (combobox == eap_method_combobox_) {
    RefreshEapFields();
  }
  UpdateDialogButtons();
  UpdateErrorLabel();
}

void WifiConfigView::OnCertificatesLoaded(bool initial_load) {
  RefreshEapFields();
  UpdateDialogButtons();
  UpdateErrorLabel();
}

bool WifiConfigView::Login() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (service_path_.empty()) {
    const bool share_default = true;  // share networks by default
    if (!eap_method_combobox_) {
      // Hidden ordinary Wi-Fi connection.
      ConnectionSecurity security = SECURITY_UNKNOWN;
      switch (security_combobox_->selected_index()) {
        case SECURITY_INDEX_NONE:
          security = SECURITY_NONE;
          break;
        case SECURITY_INDEX_WEP:
          security = SECURITY_WEP;
          break;
        case SECURITY_INDEX_PSK:
          security = SECURITY_PSK;
          break;
      }
      cros->ConnectToUnconfiguredWifiNetwork(
          GetSsid(),
          security,
          GetPassphrase(),
          NULL,
          GetSaveCredentials(),
          GetShareNetwork(share_default));
    } else {
      // Hidden 802.1X EAP Wi-Fi connection.
      chromeos::NetworkLibrary::EAPConfigData config_data;
      config_data.method = GetEapMethod();
      config_data.auth = GetEapPhase2Auth();
      config_data.server_ca_cert_nss_nickname = GetEapServerCaCertNssNickname();
      config_data.use_system_cas = GetEapUseSystemCas();
      config_data.client_cert_pkcs11_id = GetEapClientCertPkcs11Id();
      config_data.identity = GetEapIdentity();
      config_data.anonymous_identity = GetEapAnonymousIdentity();
      cros->ConnectToUnconfiguredWifiNetwork(
          GetSsid(),
          SECURITY_8021X,
          GetPassphrase(),
          &config_data,
          GetSaveCredentials(),
          GetShareNetwork(share_default));
    }
  } else {
    WifiNetwork* wifi = cros->FindWifiNetworkByPath(service_path_);
    if (!wifi) {
      // Shill no longer knows about this wifi network (edge case).
      // TODO(stevenjb): Add a notification (chromium-os13225).
      LOG(WARNING) << "Wifi network: " << service_path_ << " no longer exists.";
      return true;
    }
    if (eap_method_combobox_) {
      // Visible 802.1X EAP Wi-Fi connection.
      EAPMethod method = GetEapMethod();
      DCHECK(method != EAP_METHOD_UNKNOWN);
      wifi->SetEAPMethod(method);
      wifi->SetEAPPhase2Auth(GetEapPhase2Auth());
      wifi->SetEAPServerCaCertNssNickname(GetEapServerCaCertNssNickname());
      wifi->SetEAPUseSystemCAs(GetEapUseSystemCas());
      wifi->SetEAPClientCertPkcs11Id(GetEapClientCertPkcs11Id());
      wifi->SetEAPIdentity(GetEapIdentity());
      wifi->SetEAPAnonymousIdentity(GetEapAnonymousIdentity());
      wifi->SetEAPPassphrase(GetPassphrase());
      wifi->SetSaveCredentials(GetSaveCredentials());
    } else {
      // Visible ordinary Wi-Fi connection.
      const std::string passphrase = GetPassphrase();
      if (passphrase != wifi->passphrase())
        wifi->SetPassphrase(passphrase);
    }
    bool share_default = (wifi->profile_type() != PROFILE_USER);
    wifi->SetEnrollmentDelegate(
        CreateEnrollmentDelegate(GetWidget()->GetNativeWindow(),
                                 wifi->name(),
                                 ProfileManager::GetLastUsedProfile()));
    cros->ConnectToWifiNetwork(wifi, GetShareNetwork(share_default));
    // Connection failures are responsible for updating the UI, including
    // reopening dialogs.
  }
  return true;  // dialog will be closed
}

std::string WifiConfigView::GetSsid() const {
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

bool WifiConfigView::GetSaveCredentials() const {
  if (!save_credentials_checkbox_)
    return true;  // share networks by default (e.g. non 8021x).
  return save_credentials_checkbox_->checked();
}

bool WifiConfigView::GetShareNetwork(bool share_default) const {
  if (!share_network_checkbox_)
    return share_default;
  return share_network_checkbox_->checked();
}

EAPMethod WifiConfigView::GetEapMethod() const {
  DCHECK(eap_method_combobox_);
  switch (eap_method_combobox_->selected_index()) {
    case EAP_METHOD_INDEX_NONE:
      return EAP_METHOD_UNKNOWN;
    case EAP_METHOD_INDEX_PEAP:
      return EAP_METHOD_PEAP;
    case EAP_METHOD_INDEX_TLS:
      return EAP_METHOD_TLS;
    case EAP_METHOD_INDEX_TTLS:
      return EAP_METHOD_TTLS;
    case EAP_METHOD_INDEX_LEAP:
      return EAP_METHOD_LEAP;
    default:
      return EAP_METHOD_UNKNOWN;
  }
}

EAPPhase2Auth WifiConfigView::GetEapPhase2Auth() const {
  DCHECK(phase_2_auth_combobox_);
  switch (phase_2_auth_combobox_->selected_index()) {
    case PHASE_2_AUTH_INDEX_AUTO:
      return EAP_PHASE_2_AUTH_AUTO;
    case PHASE_2_AUTH_INDEX_MD5:
      return EAP_PHASE_2_AUTH_MD5;
    case PHASE_2_AUTH_INDEX_MSCHAPV2:
      return EAP_PHASE_2_AUTH_MSCHAPV2;
    case PHASE_2_AUTH_INDEX_MSCHAP:
      return EAP_PHASE_2_AUTH_MSCHAP;
    case PHASE_2_AUTH_INDEX_PAP:
      return EAP_PHASE_2_AUTH_PAP;
    case PHASE_2_AUTH_INDEX_CHAP:
      return EAP_PHASE_2_AUTH_CHAP;
    default:
      return EAP_PHASE_2_AUTH_AUTO;
  }
}

std::string WifiConfigView::GetEapServerCaCertNssNickname() const {
  DCHECK(server_ca_cert_combobox_);
  DCHECK(cert_library_);
  int index = server_ca_cert_combobox_->selected_index();
  if (index == 0) {
    // First item is "Default".
    return std::string();
  } else if (index == server_ca_cert_combobox_->model()->GetItemCount() - 1) {
    // Last item is "Do not check".
    return std::string();
  } else {
    DCHECK(cert_library_);
    int cert_index = index - 1;
    return cert_library_->GetCACertificates().GetNicknameAt(cert_index);
  }
}

bool WifiConfigView::GetEapUseSystemCas() const {
  DCHECK(server_ca_cert_combobox_);
  // Only use system CAs if the first item ("Default") is selected.
  return server_ca_cert_combobox_->selected_index() == 0;
}

std::string WifiConfigView::GetEapClientCertPkcs11Id() const {
  DCHECK(user_cert_combobox_);
  DCHECK(cert_library_);
  if (!HaveUserCerts()) {
    return std::string();  // "None installed"
  } else {
    // Certificates are listed in the order they appear in the model.
    int index = user_cert_combobox_->selected_index();
    return cert_library_->GetUserCertificates().GetPkcs11IdAt(index);
  }
}

std::string WifiConfigView::GetEapIdentity() const {
  DCHECK(identity_textfield_);
  return UTF16ToUTF8(identity_textfield_->text());
}

std::string WifiConfigView::GetEapAnonymousIdentity() const {
  DCHECK(identity_anonymous_textfield_);
  return UTF16ToUTF8(identity_anonymous_textfield_->text());
}

void WifiConfigView::Cancel() {
}

// This will initialize the view depending on if we have a wifi network or not.
// And if we are doing simple password encryption or the more complicated
// 802.1x encryption.
// If we are creating the "Join other network..." dialog, we will allow user
// to enter the data. And if they select the 802.1x encryption, we will show
// the 802.1x fields.
void WifiConfigView::Init(WifiNetwork* wifi, bool show_8021x) {
  if (wifi) {
    ParseWiFiEAPUIProperty(&eap_method_ui_data_, wifi, onc::eap::kOuter);
    ParseWiFiEAPUIProperty(&phase_2_auth_ui_data_, wifi, onc::eap::kInner);
    ParseWiFiEAPUIProperty(&user_cert_ui_data_, wifi, onc::eap::kClientCertRef);
    ParseWiFiEAPUIProperty(&server_ca_cert_ui_data_, wifi,
                           onc::eap::kServerCARef);
    if (server_ca_cert_ui_data_.managed()) {
      ParseWiFiEAPUIProperty(&server_ca_cert_ui_data_, wifi,
                             onc::eap::kUseSystemCAs);
    }
    ParseWiFiEAPUIProperty(&identity_ui_data_, wifi, onc::eap::kIdentity);
    ParseWiFiEAPUIProperty(&identity_anonymous_ui_data_, wifi,
                           onc::eap::kAnonymousIdentity);
    ParseWiFiEAPUIProperty(&save_credentials_ui_data_, wifi,
                           onc::eap::kSaveCredentials);
    if (show_8021x)
      ParseWiFiEAPUIProperty(&passphrase_ui_data_, wifi, onc::eap::kPassword);
    else
      ParseWiFiUIProperty(&passphrase_ui_data_, wifi, onc::wifi::kPassphrase);
  }

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  const int kPasswordVisibleWidth = 20;
  // Label
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Textfield, combobox.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0,
                        ChildNetworkConfigView::kInputFieldMinWidth);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Password visible button / policy indicator.
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, kPasswordVisibleWidth);

  // Title
  layout->StartRow(0, column_view_set_id);
  views::Label* title = new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_JOIN_WIFI_NETWORKS));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  layout->AddView(title, 5, 1);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  // SSID input
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID)));
  if (!wifi) {
    ssid_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    ssid_textfield_->SetController(this);
    ssid_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID));
    layout->AddView(ssid_textfield_);
  } else {
    views::Label* label = new views::Label(UTF8ToUTF16(wifi->name()));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(label);
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Security select
  if (!wifi && !show_8021x) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SECURITY)));
    security_combobox_model_.reset(new internal::SecurityComboboxModel);
    security_combobox_ = new views::Combobox(security_combobox_model_.get());
    security_combobox_->set_listener(this);
    layout->AddView(security_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Only enumerate certificates in the data model for 802.1X networks.
  if (show_8021x) {
    // Initialize cert_library_ for 802.1X netoworks.
    cert_library_ = chromeos::CrosLibrary::Get()->GetCertLibrary();
    // Setup a callback if certificates are yet to be loaded,
    if (!cert_library_->CertificatesLoaded())
      cert_library_->AddObserver(this);

    // EAP method
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EAP_METHOD)));
    eap_method_combobox_model_.reset(new internal::EAPMethodComboboxModel);
    eap_method_combobox_ = new views::Combobox(
        eap_method_combobox_model_.get());
    eap_method_combobox_->set_listener(this);
    eap_method_combobox_->SetEnabled(eap_method_ui_data_.editable());
    layout->AddView(eap_method_combobox_);
    layout->AddView(new ControlledSettingIndicatorView(eap_method_ui_data_));
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Phase 2 authentication
    layout->StartRow(0, column_view_set_id);
    phase_2_auth_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PHASE_2_AUTH));
    layout->AddView(phase_2_auth_label_);
    phase_2_auth_combobox_model_.reset(
        new internal::Phase2AuthComboboxModel(eap_method_combobox_));
    phase_2_auth_combobox_ = new views::Combobox(
        phase_2_auth_combobox_model_.get());
    phase_2_auth_label_->SetEnabled(false);
    phase_2_auth_combobox_->SetEnabled(false);
    phase_2_auth_combobox_->set_listener(this);
    layout->AddView(phase_2_auth_combobox_);
    layout->AddView(new ControlledSettingIndicatorView(phase_2_auth_ui_data_));
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Server CA certificate
    layout->StartRow(0, column_view_set_id);
    server_ca_cert_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA));
    layout->AddView(server_ca_cert_label_);
    server_ca_cert_combobox_model_.reset(
        new internal::ServerCACertComboboxModel(cert_library_));
    server_ca_cert_combobox_ = new ComboboxWithWidth(
        server_ca_cert_combobox_model_.get(),
        ChildNetworkConfigView::kInputFieldMinWidth);
    server_ca_cert_label_->SetEnabled(false);
    server_ca_cert_combobox_->SetEnabled(false);
    server_ca_cert_combobox_->set_listener(this);
    layout->AddView(server_ca_cert_combobox_);
    layout->AddView(
        new ControlledSettingIndicatorView(server_ca_cert_ui_data_));
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // User certificate
    layout->StartRow(0, column_view_set_id);
    user_cert_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT));
    layout->AddView(user_cert_label_);
    user_cert_combobox_model_.reset(
        new internal::UserCertComboboxModel(cert_library_));
    user_cert_combobox_ = new views::Combobox(user_cert_combobox_model_.get());
    user_cert_label_->SetEnabled(false);
    user_cert_combobox_->SetEnabled(false);
    user_cert_combobox_->set_listener(this);
    layout->AddView(user_cert_combobox_);
    layout->AddView(new ControlledSettingIndicatorView(user_cert_ui_data_));
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    // Identity
    layout->StartRow(0, column_view_set_id);
    identity_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY));
    layout->AddView(identity_label_);
    identity_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_textfield_->SetController(this);
    if (wifi && !wifi->identity().empty())
      identity_textfield_->SetText(UTF8ToUTF16(wifi->identity()));
    identity_textfield_->SetEnabled(identity_ui_data_.editable());
    layout->AddView(identity_textfield_);
    layout->AddView(new ControlledSettingIndicatorView(identity_ui_data_));
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Passphrase input
  layout->StartRow(0, column_view_set_id);
  int label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE;
  passphrase_label_ = new views::Label(
      l10n_util::GetStringUTF16(label_text_id));
  layout->AddView(passphrase_label_);
  passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_OBSCURED);
  passphrase_textfield_->SetController(this);
  if (wifi && !wifi->GetPassphrase().empty())
    passphrase_textfield_->SetText(UTF8ToUTF16(wifi->GetPassphrase()));
  // Disable passphrase input initially for other network.
  passphrase_label_->SetEnabled(wifi != NULL);
  passphrase_textfield_->SetEnabled(wifi && passphrase_ui_data_.editable());
  passphrase_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      label_text_id));
  layout->AddView(passphrase_textfield_);

  if (passphrase_ui_data_.managed()) {
    layout->AddView(new ControlledSettingIndicatorView(passphrase_ui_data_));
  } else {
    // Password visible button.
    passphrase_visible_button_ = new views::ToggleImageButton(this);
    passphrase_visible_button_->SetTooltipText(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE_SHOW));
    passphrase_visible_button_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE_HIDE));
    passphrase_visible_button_->SetImage(
        views::ImageButton::STATE_NORMAL,
        ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_NETWORK_SHOW_PASSWORD));
    passphrase_visible_button_->SetImage(
        views::ImageButton::STATE_HOVERED,
        ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_NETWORK_SHOW_PASSWORD_HOVER));
    passphrase_visible_button_->SetToggledImage(
        views::ImageButton::STATE_NORMAL,
        ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_NETWORK_HIDE_PASSWORD));
    passphrase_visible_button_->SetToggledImage(
        views::ImageButton::STATE_HOVERED,
        ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_NETWORK_HIDE_PASSWORD_HOVER));
    passphrase_visible_button_->SetImageAlignment(
        views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
    layout->AddView(passphrase_visible_button_);
  }

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  if (show_8021x) {
    // Anonymous identity
    layout->StartRow(0, column_view_set_id);
    identity_anonymous_label_ =
        new views::Label(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY_ANONYMOUS));
    layout->AddView(identity_anonymous_label_);
    identity_anonymous_textfield_ = new views::Textfield(
        views::Textfield::STYLE_DEFAULT);
    identity_anonymous_label_->SetEnabled(false);
    identity_anonymous_textfield_->SetEnabled(false);
    identity_anonymous_textfield_->SetController(this);
    layout->AddView(identity_anonymous_textfield_);
    layout->AddView(
        new ControlledSettingIndicatorView(identity_anonymous_ui_data_));
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // Checkboxes.

  // Save credentials
  if (show_8021x) {
    layout->StartRow(0, column_view_set_id);
    save_credentials_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SAVE_CREDENTIALS));
    save_credentials_checkbox_->SetEnabled(
        save_credentials_ui_data_.editable());
    layout->SkipColumns(1);
    layout->AddView(save_credentials_checkbox_);
    layout->AddView(
        new ControlledSettingIndicatorView(save_credentials_ui_data_));
  }

  // Share network
  if (!wifi ||
      (wifi->profile_type() == PROFILE_NONE &&
       wifi->IsPassphraseRequired() &&
       !wifi->RequiresUserProfile())) {
    layout->StartRow(0, column_view_set_id);
    share_network_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHARE_NETWORK));
    layout->SkipColumns(1);
    layout->AddView(share_network_checkbox_);
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Create an error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_label_->SetEnabledColor(SK_ColorRED);
  layout->AddView(error_label_);

  // Initialize the field and checkbox values.

  // After creating the fields, we set the values. Fields need to be created
  // first because RefreshEapFields() will enable/disable them as appropriate.
  if (show_8021x) {
    EAPMethod eap_method = (wifi ? wifi->eap_method() : EAP_METHOD_UNKNOWN);
    switch (eap_method) {
      case EAP_METHOD_PEAP:
        eap_method_combobox_->SetSelectedIndex(EAP_METHOD_INDEX_PEAP);
        break;
      case EAP_METHOD_TTLS:
        eap_method_combobox_->SetSelectedIndex(EAP_METHOD_INDEX_TTLS);
        break;
      case EAP_METHOD_TLS:
        eap_method_combobox_->SetSelectedIndex(EAP_METHOD_INDEX_TLS);
        break;
      case EAP_METHOD_LEAP:
        eap_method_combobox_->SetSelectedIndex(EAP_METHOD_INDEX_LEAP);
        break;
      default:
        break;
    }
    RefreshEapFields();

    // Phase 2 authentication and anonymous identity.
    if (Phase2AuthActive()) {
      EAPPhase2Auth eap_phase_2_auth =
          (wifi ? wifi->eap_phase_2_auth() : EAP_PHASE_2_AUTH_AUTO);
      switch (eap_phase_2_auth) {
        case EAP_PHASE_2_AUTH_MD5:
          phase_2_auth_combobox_->SetSelectedIndex(PHASE_2_AUTH_INDEX_MD5);
          break;
        case EAP_PHASE_2_AUTH_MSCHAPV2:
          phase_2_auth_combobox_->SetSelectedIndex(PHASE_2_AUTH_INDEX_MSCHAPV2);
          break;
        case EAP_PHASE_2_AUTH_MSCHAP:
          phase_2_auth_combobox_->SetSelectedIndex(PHASE_2_AUTH_INDEX_MSCHAP);
          break;
        case EAP_PHASE_2_AUTH_PAP:
          phase_2_auth_combobox_->SetSelectedIndex(PHASE_2_AUTH_INDEX_PAP);
          break;
        case EAP_PHASE_2_AUTH_CHAP:
          phase_2_auth_combobox_->SetSelectedIndex(PHASE_2_AUTH_INDEX_CHAP);
          break;
        default:
          break;
      }

      const std::string& eap_anonymous_identity =
          (wifi ? wifi->eap_anonymous_identity() : std::string());
      identity_anonymous_textfield_->SetText(
          UTF8ToUTF16(eap_anonymous_identity));
    }

    // Server CA certificate.
    if (CaCertActive()) {
      const std::string& nss_nickname =
          (wifi ? wifi->eap_server_ca_cert_nss_nickname() : std::string());
      if (nss_nickname.empty()) {
        if (wifi->eap_use_system_cas()) {
          // "Default".
          server_ca_cert_combobox_->SetSelectedIndex(0);
        } else {
          // "Do not check".
          server_ca_cert_combobox_->SetSelectedIndex(
              server_ca_cert_combobox_->model()->GetItemCount() - 1);
        }
      } else {
        // Select the certificate if available.
        int cert_index =
            cert_library_->GetCACertificates().FindCertByNickname(nss_nickname);
        if (cert_index >= 0) {
          // Skip item for "Default".
          server_ca_cert_combobox_->SetSelectedIndex(1 + cert_index);
        }
      }
    }

    // User certificate.
    if (UserCertActive()) {
      const std::string& pkcs11_id =
          (wifi ? wifi->eap_client_cert_pkcs11_id() : std::string());
      if (!pkcs11_id.empty()) {
        int cert_index =
            cert_library_->GetUserCertificates().FindCertByPkcs11Id(pkcs11_id);
        if (cert_index >= 0) {
          user_cert_combobox_->SetSelectedIndex(cert_index);
        }
      }
    }

    // Identity is always active.
    const std::string& eap_identity =
        (wifi ? wifi->eap_identity() : std::string());
    identity_textfield_->SetText(UTF8ToUTF16(eap_identity));

    // Passphrase
    if (PassphraseActive()) {
      const std::string& eap_passphrase =
          (wifi ? wifi->eap_passphrase() : std::string());
      passphrase_textfield_->SetText(UTF8ToUTF16(eap_passphrase));
    }

    // Save credentials
    bool save_credentials = (wifi ? wifi->save_credentials() : false);
    save_credentials_checkbox_->SetChecked(save_credentials);
  }

  RefreshShareCheckbox();
  UpdateErrorLabel();
}

void WifiConfigView::InitFocus() {
  views::View* view_to_focus = GetInitiallyFocusedView();
  if (view_to_focus)
    view_to_focus->RequestFocus();
}

// static
void WifiConfigView::ParseWiFiUIProperty(
    NetworkPropertyUIData* property_ui_data,
    Network* network,
    const std::string& key) {
  NetworkLibrary* network_library = CrosLibrary::Get()->GetNetworkLibrary();
  property_ui_data->ParseOncProperty(
      network->ui_data(),
      network_library->FindOncForNetwork(network->unique_id()),
      base::StringPrintf("%s.%s", onc::network_config::kWiFi, key.c_str()));
}

// static
void WifiConfigView::ParseWiFiEAPUIProperty(
    NetworkPropertyUIData* property_ui_data,
    Network* network,
    const std::string& key) {
  ParseWiFiUIProperty(
      property_ui_data, network,
      base::StringPrintf("%s.%s", onc::wifi::kEAP, key.c_str()));
}

}  // namespace chromeos
