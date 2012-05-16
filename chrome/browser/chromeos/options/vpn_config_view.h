// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_VPN_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_VPN_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/options/passphrase_textfield.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace chromeos {

namespace internal {
class ProviderTypeComboboxModel;
class VpnServerCACertComboboxModel;
class VpnUserCertComboboxModel;
}

// A dialog box to allow configuration of VPN connection.
class VPNConfigView : public ChildNetworkConfigView,
                      public views::TextfieldController,
                      public views::ButtonListener,
                      public views::ComboboxListener,
                      public CertLibrary::Observer {
 public:
  VPNConfigView(NetworkConfigView* parent, VirtualNetwork* vpn);
  explicit VPNConfigView(NetworkConfigView* parent);
  virtual ~VPNConfigView();

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::ComboboxListener:
  virtual void OnSelectedIndexChanged(views::Combobox* combobox) OVERRIDE;

  // CertLibrary::Observer:
  virtual void OnCertificatesLoaded(bool initial_load) OVERRIDE;

  // ChildNetworkConfigView:
  virtual string16 GetTitle() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool CanLogin() OVERRIDE;
  virtual bool Login() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void InitFocus() OVERRIDE;

 private:
  // Initializes data members and create UI controls.
  void Init(VirtualNetwork* vpn);

  // Set and update all control values.
  void Refresh();

  // Update various controls.
  void UpdateControlsToEnable();
  void UpdateControls();
  void UpdateErrorLabel();

  // Update state of the Login button.
  void UpdateCanLogin();

  // Returns true if the provider type requires a user certificate.
  bool UserCertRequired() const;

  // Returns true if there is at least one user certificate installed.
  bool HaveUserCerts() const;

  // Returns true if there is a selected user certificate and it is valid.
  bool IsUserCertValid() const;

  // Get text from input field.
  const std::string GetTextFromField(views::Textfield* textfield,
                                     bool trim_whitespace) const;

  // Get passphrase from input field.
  const std::string GetPassphraseFromField(
      PassphraseTextfield* textfield) const;

  // Convenience methods to get text from input field or cached VirtualNetwork.
  const std::string GetService() const;
  const std::string GetServer() const;
  const std::string GetPSKPassphrase() const;
  const std::string GetUsername() const;
  const std::string GetUserPassphrase() const;
  const std::string GetOTP() const;
  const std::string GetGroupName() const;
  const std::string GetServerCACertNssNickname() const;
  const std::string GetUserCertID() const;

  // Parses a VPN UI |property| from the given |network|. |key| is the property
  // name within the type-specific VPN subdictionary named |dict_key|.
  void ParseVPNUIProperty(NetworkPropertyUIData* property_ui_data,
                          Network* network,
                          const std::string& dict_key,
                          const std::string& key);

  CertLibrary* cert_library_;

  std::string server_hostname_;
  string16 service_name_from_server_;
  bool service_text_modified_;

  // Initialized in Init():

  ProviderType provider_type_;

  bool enable_psk_passphrase_;
  bool enable_user_cert_;
  bool enable_server_ca_cert_;
  bool enable_otp_;
  bool enable_group_name_;

  NetworkPropertyUIData ca_cert_ui_data_;
  NetworkPropertyUIData psk_passphrase_ui_data_;
  NetworkPropertyUIData user_cert_ui_data_;
  NetworkPropertyUIData username_ui_data_;
  NetworkPropertyUIData user_passphrase_ui_data_;
  NetworkPropertyUIData group_name_ui_data_;

  views::Textfield* server_textfield_;
  views::Label* service_text_;
  views::Textfield* service_textfield_;
  scoped_ptr<internal::ProviderTypeComboboxModel> provider_type_combobox_model_;
  views::Combobox* provider_type_combobox_;
  views::Label* provider_type_text_label_;
  views::Label* psk_passphrase_label_;
  PassphraseTextfield* psk_passphrase_textfield_;
  views::Label* user_cert_label_;
  scoped_ptr<internal::VpnUserCertComboboxModel> user_cert_combobox_model_;
  views::Combobox* user_cert_combobox_;
  views::Label* server_ca_cert_label_;
  scoped_ptr<internal::VpnServerCACertComboboxModel>
      server_ca_cert_combobox_model_;
  views::Combobox* server_ca_cert_combobox_;
  views::Textfield* username_textfield_;
  PassphraseTextfield* user_passphrase_textfield_;
  views::Label* otp_label_;
  views::Textfield* otp_textfield_;
  views::Label* group_name_label_;
  views::Textfield* group_name_textfield_;
  views::Label* error_label_;

  DISALLOW_COPY_AND_ASSIGN(VPNConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_VPN_CONFIG_VIEW_H_
