// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/views/dropdown_button.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace views {
class Combobox;
class GridLayout;
class Label;
class NativeButton;
class Throbber;
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

  // Returns top level native window for the view.
  gfx::NativeWindow GetNativeWindow() const;

  // Returns network control view.
  views::View* GetNetworkControlView() const;

  // Shows network connecting status or network selection otherwise.
  void ShowConnectingStatus(bool connecting, const string16& network_id);

  // Returns true if only throbber is visible, the view is in waiting status.
  bool IsConnecting() const;

  // Sets whether continue control is enabled.
  void EnableContinue(bool enabled);

  // Returns whether continue button is enabled.
  bool IsContinueEnabled() const;

  // views::LinkController implementation.
  virtual void LinkActivated(views::Link* source, int);

  // Returns true if any dialog box is currently open?
  bool is_dialog_open() const {
    return proxy_settings_dialog_.get() && proxy_settings_dialog_->is_open();
  }

 protected:
  // Overridden from views::View.
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual void OnLocaleChanged();
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
    return true;
  }

  // LoginHtmlDialog::Delegate implementation:
  virtual void OnDialogClosed() {}

 private:
  // Add screen controls to the contents layout specified.
  // Based on state (connecting to the network or not)
  // different controls are added.
  void AddControlsToLayout(views::GridLayout* contents_layout);

  // Initializes grid layout of the screen. Called on language change too.
  void InitLayout();

  // Delete and recreate native controls that
  // fail to update preferred size after string update.
  void RecreateNativeControls();

  // Updates text on label with currently connecting network.
  void UpdateConnectingNetworkLabel();

  // View that defines FillLayout for the whole screen (contents & title).
  views::View* entire_screen_view_;

  // View that contains screen contents (except title).
  views::View* contents_view_;

  // Screen controls.
  DropDownButton* languages_menubutton_;
  DropDownButton* keyboards_menubutton_;
  views::Label* welcome_label_;
  views::Label* select_language_label_;
  views::Label* select_keyboard_label_;
  views::Label* select_network_label_;
  views::Label* connecting_network_label_;
  NetworkDropdownButton* network_dropdown_;
  views::NativeButton* continue_button_;
  views::Throbber* throbber_;
  views::Link* proxy_settings_link_;
  bool show_keyboard_button_;

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
