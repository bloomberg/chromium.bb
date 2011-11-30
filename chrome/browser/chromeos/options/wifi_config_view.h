// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
class ToggleImageButton;
class Label;
}

namespace chromeos {

// A dialog box for showing a password textfield.
class WifiConfigView : public ChildNetworkConfigView,
                       public views::TextfieldController,
                       public views::ButtonListener,
                       public views::ComboboxListener,
                       public CertLibrary::Observer {
 public:
  // Wifi login dialog for wifi network |wifi|. |wifi| must be a non NULL
  // pointer to a WifiNetwork in NetworkLibrary.
  WifiConfigView(NetworkConfigView* parent, WifiNetwork* wifi);
  // Wifi login dialog for "Joining other network..."
  WifiConfigView(NetworkConfigView* parent, bool show_8021x);
  virtual ~WifiConfigView();

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::ComboboxListener:
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index,
                           int new_index) OVERRIDE;

  // CertLibrary::Observer:
  virtual void OnCertificatesLoaded(bool initial_load) OVERRIDE;

  // ChildNetworkConfigView:
  virtual string16 GetTitle() OVERRIDE;
  virtual bool CanLogin() OVERRIDE;
  virtual bool Login() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void InitFocus() OVERRIDE;

 private:
  // Initializes UI.  If |show_8021x| includes 802.1x config options.
  void Init(WifiNetwork* wifi, bool show_8021x);

  // Get input values.
  std::string GetSsid() const;
  std::string GetPassphrase() const;
  bool GetSaveCredentials() const;
  bool GetShareNetwork(bool share_default) const;

  // Get various 802.1X EAP values from the widgets.
  EAPMethod GetEapMethod() const;
  EAPPhase2Auth GetEapPhase2Auth() const;
  std::string GetEapServerCaCertNssNickname() const;
  bool GetEapUseSystemCas() const;
  std::string GetEapClientCertPkcs11Id() const;
  std::string GetEapIdentity() const;
  std::string GetEapAnonymousIdentity() const;

  // Returns true if the EAP method requires a user certificate.
  bool UserCertRequired() const;

  // Returns true if at least one user certificate is installed.
  bool HaveUserCerts() const;

  // Returns true if there is a selected user certificate and it is valid.
  bool IsUserCertValid() const;

  // Updates state of the Login button.
  void UpdateDialogButtons();

  // Enable/Disable EAP fields as appropriate based on selected EAP method.
  void RefreshEapFields();

  // Enable/Disable "share this network" checkbox.
  void RefreshShareCheckbox();

  // Updates the error text label.
  void UpdateErrorLabel();

  CertLibrary* cert_library_;

  views::Textfield* ssid_textfield_;
  views::Combobox* eap_method_combobox_;
  views::Label* phase_2_auth_label_;
  views::Combobox* phase_2_auth_combobox_;
  views::Label* user_cert_label_;
  views::Combobox* user_cert_combobox_;
  views::Label* server_ca_cert_label_;
  views::Combobox* server_ca_cert_combobox_;
  views::Label* identity_label_;
  views::Textfield* identity_textfield_;
  views::Label* identity_anonymous_label_;
  views::Textfield* identity_anonymous_textfield_;
  views::Checkbox* save_credentials_checkbox_;
  views::Checkbox* share_network_checkbox_;
  views::Label* shared_network_label_;
  views::Combobox* security_combobox_;
  views::Label* passphrase_label_;
  views::Textfield* passphrase_textfield_;
  views::ToggleImageButton* passphrase_visible_button_;
  views::Label* error_label_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
