// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "ui/base/models/combobox_model.h"
#include "views/controls/button/button.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"

class FilePath;

namespace chromeos {

class NetworkConfigView;

// A dialog box for showing a password textfield.
class WifiConfigView : public views::View,
                       public views::TextfieldController,
                       public views::ButtonListener,
                       public views::Combobox::Listener,
                       public SelectFileDialog::Listener {
 public:
  // Wifi login dialog for wifi network |wifi|
  WifiConfigView(NetworkConfigView* parent, WifiNetwork* wifi);
  // Wifi login dialog for "Joining other network..."
  explicit WifiConfigView(NetworkConfigView* parent);
  virtual ~WifiConfigView();

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event);

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* combo_box,
                           int prev_index, int new_index);

  // SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // Login to network. Returns false if the dialog should remain open.
  virtual bool Login();

  // Cancel the dialog.
  virtual void Cancel();

  // Get the typed in ssid.
  const std::string GetSSID() const;
  // Get the typed in passphrase.
  const std::string GetPassphrase() const;

  // Returns whether or not we can login.
  bool CanLogin();

 private:
  // Initializes UI.
  void Init();

  // Updates state of the Login button.
  void UpdateDialogButtons();

  // Updates the error text label.
  void UpdateErrorLabel(bool failed);

  NetworkConfigView* parent_;

  // Whether or not it is an 802.1x network.
  bool is_8021x_;

  WifiNetwork* wifi_;

  views::Textfield* ssid_textfield_;
  views::Combobox* eap_method_combobox_;
  views::Combobox* phase_2_auth_combobox_;
  views::Textfield* identity_textfield_;
  views::Textfield* identity_anonymous_textfield_;
  views::NativeButton* certificate_browse_button_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  std::string certificate_path_;
  views::Combobox* security_combobox_;
  views::Textfield* passphrase_textfield_;
  views::ImageButton* passphrase_visible_button_;
  views::Label* error_label_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_VIEW_H_
