// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/vpn_config_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/cert_database.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

namespace {

string16 ProviderTypeToString(chromeos::VirtualNetwork::ProviderType type) {
  switch (type) {
    case chromeos::VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_PSK);
    case chromeos::VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_USER_CERT);
    case chromeos::VirtualNetwork::PROVIDER_TYPE_OPEN_VPN:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_OPEN_VPN);
    case chromeos::VirtualNetwork::PROVIDER_TYPE_MAX:
      break;
  }
  NOTREACHED();
  return string16();
}

}  // namespace

namespace chromeos {

class ProviderTypeComboboxModel : public ui::ComboboxModel {
 public:
  ProviderTypeComboboxModel() {}
  virtual ~ProviderTypeComboboxModel() {}
  virtual int GetItemCount() {
    // TODO(stevenjb): Include OpenVPN option once enabled.
    return VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT + 1;
    // return VirtualNetwork::PROVIDER_TYPE_MAX;
  }
  virtual string16 GetItemAt(int index) {
    VirtualNetwork::ProviderType type =
        static_cast<VirtualNetwork::ProviderType>(index);
    return ProviderTypeToString(type);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ProviderTypeComboboxModel);
};

class UserCertComboboxModel : public ui::ComboboxModel {
 public:
  UserCertComboboxModel() {
    net::CertDatabase cert_db;
    net::CertificateList cert_list;
    cert_db.ListCerts(&cert_list);
    // Find all the user certificates.  There shouldn't be many.
    for (net::CertificateList::const_iterator it = cert_list.begin();
        it != cert_list.end(); ++it) {
      net::X509Certificate* cert = it->get();
      net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
      net::CertType type = x509_certificate_model::GetType(cert_handle);
      if (type == net::USER_CERT)
        user_certs_.push_back(*it);
    }
  }
  virtual ~UserCertComboboxModel() {}

  virtual int GetItemCount() {
    if (user_certs_.empty())
      return 1;  // "None installed" item
    return static_cast<int>(user_certs_.size());
  }

  virtual string16 GetItemAt(int index) {
    if (user_certs_.empty()) {
      // "None installed" item.
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USER_CERT_NONE_INSTALLED);
    }
    if (index >= 0 && index < static_cast<int>(user_certs_.size())) {
      net::X509Certificate* cert = user_certs_[index].get();
      std::string name =
          x509_certificate_model::GetCertNameOrNickname(cert->os_cert_handle());
      return UTF8ToUTF16(name);
    }
    return string16();
  }

  bool HaveCerts() {
    return !user_certs_.empty();
  }

  std::string GetCertID(int index) {
    if (0 <= index && index < static_cast<int>(user_certs_.size())) {
      // TODO(jamescook): Return the proper ID for flimflam, perhaps PKCS#11?
      return "/tmp/ca.pem";
    }
    return std::string();
  }

 private:
  net::CertificateList user_certs_;
  DISALLOW_COPY_AND_ASSIGN(UserCertComboboxModel);
};

VPNConfigView::VPNConfigView(NetworkConfigView* parent, VirtualNetwork* vpn)
    : ChildNetworkConfigView(parent, vpn) {
  Init(vpn);
}

VPNConfigView::VPNConfigView(NetworkConfigView* parent)
    : ChildNetworkConfigView(parent) {
  Init(NULL);
}

VPNConfigView::~VPNConfigView() {
}

void VPNConfigView::UpdateCanLogin() {
  parent_->GetDialogClientView()->UpdateDialogButtons();
}

string16 VPNConfigView::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_ADD_VPN);
}

bool VPNConfigView::CanLogin() {
  // TODO(stevenjb): min kMinPassphraseLen length?
  if (service_path_.empty() &&
      (GetService().empty() || GetServer().empty()))
    return false;
  if (UserCertRequired() && GetUserCertID().empty())
    return false;
  if (GetUsername().empty())
    return false;
  return true;
}

void VPNConfigView::UpdateErrorLabel() {
  std::string error_msg;
  if (UserCertRequired() && GetUserCertID().empty())
    error_msg = l10n_util::GetStringUTF8(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PLEASE_INSTALL_USER_CERT);
  if (!service_path_.empty()) {
    // TODO(kuan): differentiate between bad psk and user passphrases.
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path_);
    if (vpn && vpn->failed()) {
      if (vpn->error() == ERROR_BAD_PASSPHRASE) {
        error_msg = l10n_util::GetStringUTF8(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE);
      } else {
        error_msg = vpn->GetErrorString();
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

bool VPNConfigView::UserCertRequired() const {
  return provider_type_ == VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT
      || provider_type_ == VirtualNetwork::PROVIDER_TYPE_OPEN_VPN;
}

void VPNConfigView::ContentsChanged(views::Textfield* sender,
                                    const string16& new_contents) {
  if (sender == server_textfield_ && !service_text_modified_) {
    // Set the service name to the server name up to '.', unless it has
    // been explicitly set by the user.
    string16 server = server_textfield_->text();
    string16::size_type n = server.find_first_of(L'.');
    service_name_from_server_ = server.substr(0, n);
    service_textfield_->SetText(service_name_from_server_);
  }
  if (sender == service_textfield_) {
    if (new_contents.empty())
      service_text_modified_ = false;
    else if (new_contents != service_name_from_server_)
      service_text_modified_ = true;
  }
  UpdateCanLogin();
}

bool VPNConfigView::HandleKeyEvent(views::Textfield* sender,
                                   const views::KeyEvent& key_event) {
  if ((sender == psk_passphrase_textfield_ ||
       sender == user_passphrase_textfield_) &&
      key_event.key_code() == ui::VKEY_RETURN) {
    parent_->GetDialogClientView()->AcceptWindow();
  }
  return false;
}

void VPNConfigView::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
}

void VPNConfigView::ItemChanged(views::Combobox* combo_box,
                                int prev_index, int new_index) {
  if (prev_index == new_index)
    return;
  if (combo_box == provider_type_combobox_) {
    provider_type_ = static_cast<VirtualNetwork::ProviderType>(new_index);
    EnableControls();
    UpdateErrorLabel();
  } else if (combo_box == user_cert_combobox_) {
    // Nothing to do for now.
  } else {
    NOTREACHED();
  }
  UpdateCanLogin();
}

bool VPNConfigView::Login() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (service_path_.empty()) {
    switch (provider_type_) {
      case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK:
        cros->ConnectToVirtualNetworkPSK(GetService(),
                                         GetServer(),
                                         GetPSKPassphrase(),
                                         GetUsername(),
                                         GetUserPassphrase());
        break;
      case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      case VirtualNetwork::PROVIDER_TYPE_OPEN_VPN:
        // TODO(stevenjb): Add support for OpenVPN and user certs.
        LOG(WARNING) << "Unsupported provider type: " << provider_type_;
        break;
      case VirtualNetwork::PROVIDER_TYPE_MAX:
        break;
    }
  } else {
    VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path_);
    if (!vpn) {
      // TODO(stevenjb): Add notification for this.
      LOG(WARNING) << "VPN no longer exists: " << service_path_;
      return true;  // Close dialog.
    }
    switch (provider_type_) {
      case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK:
        vpn->SetPSKPassphrase(GetPSKPassphrase());
        break;
      case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      case VirtualNetwork::PROVIDER_TYPE_OPEN_VPN: {
        // TODO(jamescook): Figure out if flimflam consumes a cert path,
        // PKCS#11 ID or other identifier.  Then do something like:
        // vpn->SetUserCertID(GetUserCertID());
        LOG(WARNING) << "VPN user certs not yet supported.";
        break;
      }
      case VirtualNetwork::PROVIDER_TYPE_MAX:
        break;
    }
    vpn->SetUsername(GetUsername());
    vpn->SetUserPassphrase(GetUserPassphrase());

    cros->ConnectToVirtualNetwork(vpn);
  }
  // Connection failures are responsible for updating the UI, including
  // reopening dialogs.
  return true;  // Close dialog.
}

void VPNConfigView::Cancel() {
}

void VPNConfigView::InitFocus() {
  // Put focus in the first editable field.
  if (server_textfield_)
    server_textfield_->RequestFocus();
  else if (service_textfield_)
    service_textfield_->RequestFocus();
  else if (provider_type_combobox_)
    provider_type_combobox_->RequestFocus();
  else if (psk_passphrase_textfield_ && psk_passphrase_textfield_->IsEnabled())
    psk_passphrase_textfield_->RequestFocus();
  else if (user_cert_combobox_ && user_cert_combobox_->IsEnabled())
    user_cert_combobox_->RequestFocus();
}

const std::string VPNConfigView::GetTextFromField(
    views::Textfield* textfield, bool trim_whitespace) const {
  std::string untrimmed = UTF16ToUTF8(textfield->text());
  if (!trim_whitespace)
    return untrimmed;
  std::string result;
  TrimWhitespaceASCII(untrimmed, TRIM_ALL, &result);
  return result;
}

const std::string VPNConfigView::GetService() const {
  if (service_textfield_ != NULL)
    return GetTextFromField(service_textfield_, true);
  return service_path_;
}

const std::string VPNConfigView::GetServer() const {
  if (server_textfield_ != NULL)
    return GetTextFromField(server_textfield_, true);
  return server_hostname_;
}

const std::string VPNConfigView::GetPSKPassphrase() const {
  if (psk_passphrase_textfield_->IsEnabled() &&
      psk_passphrase_textfield_->IsVisible())
    return GetTextFromField(psk_passphrase_textfield_, false);
  return std::string();
}

const std::string VPNConfigView::GetUsername() const {
  return GetTextFromField(username_textfield_, true);
}

const std::string VPNConfigView::GetUserPassphrase() const {
  return GetTextFromField(user_passphrase_textfield_, false);
}

const std::string VPNConfigView::GetUserCertID() const {
  int selected = user_cert_combobox_->selected_item();
  UserCertComboboxModel* model = static_cast<UserCertComboboxModel*>(
      user_cert_combobox_->model());
  return model->GetCertID(selected);
}

void VPNConfigView::Init(VirtualNetwork* vpn) {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  // Label.
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Textfield, combobox.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0,
                        ChildNetworkConfigView::kPassphraseWidth);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Passphrase visible button.
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  // Initialize members.
  service_text_modified_ = false;
  provider_type_ = vpn ?
      vpn->provider_type() :
      chromeos::VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK;

  // Server label and input.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVER_HOSTNAME))));
  if (!vpn) {
    server_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    server_textfield_->SetController(this);
    layout->AddView(server_textfield_);
    server_text_ = NULL;
  } else {
    server_hostname_ = vpn->server_hostname();
    server_text_ = new views::Label(UTF8ToWide(server_hostname_));
    server_text_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(server_text_);
    server_textfield_ = NULL;
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Service label and name or input.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVICE_NAME))));
  if (!vpn) {
    service_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    service_textfield_->SetController(this);
    layout->AddView(service_textfield_);
    service_text_ = NULL;
  } else {
    service_text_ = new views::Label(ASCIIToWide(vpn->name()));
    service_text_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(service_text_);
    service_textfield_ = NULL;
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Provider type label and select.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PROVIDER_TYPE))));
  if (!vpn) {
    provider_type_combobox_ =
        new views::Combobox(new ProviderTypeComboboxModel());
    provider_type_combobox_->set_listener(this);
    layout->AddView(provider_type_combobox_);
    provider_type_text_label_ = NULL;
  } else {
    provider_type_text_label_ =
        new views::Label(UTF16ToWide(ProviderTypeToString(provider_type_)));
    layout->AddView(provider_type_text_label_);
    provider_type_combobox_ = NULL;
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // PSK passphrase label, input and visible button.
  layout->StartRow(0, column_view_set_id);
  psk_passphrase_label_ =  new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PSK_PASSPHRASE)));
  layout->AddView(psk_passphrase_label_);
  psk_passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_PASSWORD);
  psk_passphrase_textfield_->SetController(this);
  if (vpn && !vpn->psk_passphrase().empty())
    psk_passphrase_textfield_->SetText(UTF8ToUTF16(vpn->psk_passphrase()));
  layout->AddView(psk_passphrase_textfield_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // User certificate label and input.
  layout->StartRow(0, column_view_set_id);
  user_cert_label_ = new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USER_CERT)));
  layout->AddView(user_cert_label_);
  user_cert_combobox_ = new views::Combobox(new UserCertComboboxModel());
  user_cert_combobox_->set_listener(this);
  if (vpn && !vpn->user_cert().empty()) {
    string16 user_cert = UTF8ToUTF16(vpn->user_cert());
    for (int i = 0; i < user_cert_combobox_->model()->GetItemCount(); ++i) {
      // TODO(jamescook):  Select the proper certificate based on the
      // appropriate property from flimflam, perhaps PKCS#11 ID.
      if (user_cert_combobox_->model()->GetItemAt(i) == user_cert) {
        user_cert_combobox_->SetSelectedItem(i);
        break;
      }
    }
  }
  layout->AddView(user_cert_combobox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Username label and input.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USERNAME))));
  username_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
  username_textfield_->SetController(this);
  if (vpn && !vpn->username().empty())
    username_textfield_->SetText(UTF8ToUTF16(vpn->username()));
  layout->AddView(username_textfield_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // User passphrase label, input and visble button.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USER_PASSPHRASE))));
  user_passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_PASSWORD);
  user_passphrase_textfield_->SetController(this);
  if (vpn && !vpn->user_passphrase().empty())
    user_passphrase_textfield_->SetText(UTF8ToUTF16(vpn->user_passphrase()));
  layout->AddView(user_passphrase_textfield_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetColor(SK_ColorRED);
  layout->AddView(error_label_);

  // Enable controls based on provider type combo.
  EnableControls();

  // Set or hide the error text.
  UpdateErrorLabel();
}

void VPNConfigView::EnableControls() {
  switch (provider_type_) {
    case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK:
      psk_passphrase_label_->SetEnabled(true);
      psk_passphrase_textfield_->SetEnabled(true);
      user_cert_label_->SetEnabled(false);
      user_cert_combobox_->SetEnabled(false);
      break;
    case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
    case VirtualNetwork::PROVIDER_TYPE_OPEN_VPN: {
      psk_passphrase_label_->SetEnabled(false);
      psk_passphrase_textfield_->SetEnabled(false);
      user_cert_label_->SetEnabled(true);
      // Only enable the combobox if the user actually has a cert to select.
      bool have_cert = !GetUserCertID().empty();
      user_cert_combobox_->SetEnabled(have_cert);
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace chromeos
