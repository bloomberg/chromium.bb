// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_

#include <string>

#include "views/controls/button/menu_button.h"
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

class NetworkScreenDelegate;
class ScreenObserver;

// View for the network selection/initial welcome screen.
class NetworkSelectionView : public views::View {
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

  // Gets/Sets the selected item in the network combobox.
  int GetSelectedNetworkItem() const;
  void SetSelectedNetworkItem(int index);

  gfx::NativeWindow GetNativeWindow();

  // Inform the network combobox that its model changed.
  void NetworkModelChanged();

  // Shows network connecting status or network selection otherwise.
  void ShowConnectingStatus(bool connecting, const string16& network_id);

  // Sets whether continue control is enabled.
  void EnableContinue(bool enabled);

 protected:
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child);
  virtual void LocaleChanged();

 private:
  // Delete and recreate native controls that
  // fail to update preferred size after string update.
  void RecreateNativeControls();

  // Updates text on label with currently connecting network.
  void UpdateConnectingNetworkLabel();

  // Dialog controls.
  views::Combobox* network_combobox_;
  views::MenuButton* languages_menubutton_;
  views::Label* welcome_label_;
  views::Label* select_language_label_;
  views::Label* select_network_label_;
  views::Label* connecting_network_label_;
  views::NativeButton* continue_button_;
  views::SmoothedThrobber* throbber_;

  // NetworkScreen delegate.
  NetworkScreenDelegate* delegate_;

  // Id of the network that is in process of connecting.
  string16 network_id_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSelectionView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
