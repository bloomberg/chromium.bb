// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_WIMAX_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_WIMAX_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/options/wifi_config_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Checkbox;
class Label;
class ToggleImageButton;
}

namespace chromeos {

// A dialog box for showing a password textfield.
class WimaxConfigView : public ChildNetworkConfigView,
                        public views::TextfieldController,
                        public views::ButtonListener {
 public:
  // Wimax login dialog for wimax network |wimax|. |wimax| must be a non NULL
  // pointer to a WimaxNetwork in NetworkLibrary.
  WimaxConfigView(NetworkConfigView* parent, WimaxNetwork* wimax);
  virtual ~WimaxConfigView();

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event) OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // ChildNetworkConfigView:
  virtual string16 GetTitle() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool CanLogin() OVERRIDE;
  virtual bool Login() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void InitFocus() OVERRIDE;

 private:
  void Init(WimaxNetwork* wimax);

  // Get input values.
  std::string GetEapIdentity() const;
  std::string GetEapPassphrase() const;
  bool GetSaveCredentials() const;
  bool GetShareNetwork(bool share_default) const;

  // Updates state of the Login button.
  void UpdateDialogButtons();

  // Enable/Disable "share this network" checkbox.
  void RefreshShareCheckbox();

  // Updates the error text label.
  void UpdateErrorLabel();

  NetworkPropertyUIData identity_ui_data_;
  NetworkPropertyUIData passphrase_ui_data_;
  NetworkPropertyUIData save_credentials_ui_data_;

  views::Label* identity_label_;
  views::Textfield* identity_textfield_;
  views::Checkbox* save_credentials_checkbox_;
  views::Checkbox* share_network_checkbox_;
  views::Label* shared_network_label_;
  views::Label* passphrase_label_;
  views::Textfield* passphrase_textfield_;
  views::ToggleImageButton* passphrase_visible_button_;
  views::Label* error_label_;

  DISALLOW_COPY_AND_ASSIGN(WimaxConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_WIMAX_CONFIG_VIEW_H_
