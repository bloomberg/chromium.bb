// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_

#include "app/combobox_model.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "views/controls/combobox/combobox.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

namespace chromeos {
class ScreenObserver;
}  // namespace chromeos

namespace views {
class Label;
}  // namespace views

// View for the network selection/initial welcome screen.
class NetworkSelectionView : public WizardScreen,
                             public views::WindowDelegate,
                             public ComboboxModel,
                             public views::Combobox::Listener,
                             public chromeos::NetworkLibrary::Observer {
 public:
  explicit NetworkSelectionView(chromeos::ScreenObserver* observer);
  virtual ~NetworkSelectionView();

  // WizardScreen implementation:
  virtual void Init();
  virtual void UpdateLocalizedStrings();

  // views::View: implementation:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // views::WindowDelegate implementation:
  virtual views::View* GetContentsView();

  // ComboboxModel implementation:
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

  // views::Combobox::Listener implementation:
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // NetworkLibrary::Observer implementation:
  virtual void NetworkChanged(chromeos::NetworkLibrary* obj);
  virtual void NetworkTraffic(chromeos::NetworkLibrary* cros, int traffic_type);

 private:
  // Returns true if there's at least one Wifi network available in the model.
  bool HasWifiNetworks();

  // Returns WiFi network by index from model.
  const chromeos::WifiNetwork& GetWifiNetworkAt(int index);

  // Dialog controls.
  views::Combobox* network_combobox_;
  views::Label* welcome_label_;
  views::Label* select_network_label_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;

  // TODO(nkostylev): Refactor network list which is also represented in
  // NetworkMenuButton, InternetPageView.
  // Cached list of WiFi networks (part of combobox model).
  chromeos::WifiNetworkVector wifi_networks_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSelectionView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_NETWORK_SELECTION_VIEW_H_
