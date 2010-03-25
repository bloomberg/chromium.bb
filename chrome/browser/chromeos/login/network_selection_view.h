// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_

#include <string>

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/language_combobox_model.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

namespace views {
class Combobox;
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

class NetworkScreenDelegate;
class ScreenObserver;

// View for the network selection/initial welcome screen.
class NetworkSelectionView : public views::View,
                             public views::ViewMenuDelegate,
                             public menus::SimpleMenuModel,
                             public menus::SimpleMenuModel::Delegate {
 public:
  NetworkSelectionView(ScreenObserver* observer,
                       NetworkScreenDelegate* delegate);
  virtual ~NetworkSelectionView();

  // Initialize view layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // views::View: implementation:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(View* source, const gfx::Point& pt);

  // menus::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Gets/Sets the selected item in the network combobox.
  int GetSelectedNetworkItem() const;
  void SetSelectedNetworkItem(int index);

  gfx::NativeWindow GetNativeWindow();

  // Inform the network combobox that its model changed.
  void NetworkModelChanged();

  // Shows network connecting status or network selection otherwise.
  void ShowConnectingStatus(bool connecting, const string16& network_id);

 private:
  // Initializes language selection menues contents.
  void InitLanguageMenu();

  // Updates text on label with currently connecting network.
  void UpdateConnectingNetworkLabel();

  // Dialog controls.
  views::Combobox* network_combobox_;
  views::MenuButton* languages_menubutton_;
  views::Label* welcome_label_;
  views::Label* select_network_label_;
  views::Label* connecting_network_label_;
  views::NativeButton* offline_button_;

  // Dialog controls that we own ourself.
  scoped_ptr<views::Menu2> languages_menu_;
  scoped_ptr<menus::SimpleMenuModel> languages_submenu_;

  // Language locale name storage.
  LanguageList languages_model_;

  // Notifications receiver.
  ScreenObserver* observer_;

  // NetworkScreen delegate.
  NetworkScreenDelegate* delegate_;

  // Id of the network that is in process of connecting.
  string16 network_id_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSelectionView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
