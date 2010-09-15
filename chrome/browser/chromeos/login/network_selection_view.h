// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

namespace views {
class Combobox;
class Label;
class NativeButton;
class SmoothedThrobber;
}  // namespace views

namespace chromeos {

class NetworkDropdownButton;
class NetworkScreenDelegate;
class ScreenObserver;

// View for the network selection/initial welcome screen.
class NetworkSelectionView : public views::View,
                             public views::LinkController,
                             public LoginHtmlDialog::Delegate {
 public:
  explicit NetworkSelectionView(NetworkScreenDelegate* delegate);
  virtual ~NetworkSelectionView();

  // Initialize view layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // views::View: implementation:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  gfx::NativeWindow GetNativeWindow();

  // Shows network connecting status or network selection otherwise.
  void ShowConnectingStatus(bool connecting, const string16& network_id);

  // Sets whether continue control is enabled.
  void EnableContinue(bool enabled);

  // views::LinkController implementation.
  virtual void LinkActivated(views::Link* source, int);

 protected:
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child);
  virtual void OnLocaleChanged();

  // LoginHtmlDialog::Delegate implementation:
  virtual void OnDialogClosed() {}

 private:
  // Delete and recreate native controls that
  // fail to update preferred size after string update.
  void RecreateNativeControls();

  // Updates text on label with currently connecting network.
  void UpdateConnectingNetworkLabel();

  // Dialog controls.
  views::MenuButton* languages_menubutton_;
  views::Label* welcome_label_;
  views::Label* select_language_label_;
  views::Label* select_network_label_;
  views::Label* connecting_network_label_;
  NetworkDropdownButton* network_dropdown_;
  views::NativeButton* continue_button_;
  views::SmoothedThrobber* throbber_;
  views::Link* proxy_settings_link_;

  // Tab index of continue button.
  int continue_button_order_index_;

  // Whether continue_button is enabled.
  bool continue_button_enabled_;

  // NetworkScreen delegate.
  NetworkScreenDelegate* delegate_;

  // Id of the network that is in process of connecting.
  string16 network_id_;

  // Dialog used for to launch proxy settings.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSelectionView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
