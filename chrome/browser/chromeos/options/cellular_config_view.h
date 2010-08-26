// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_CELLULAR_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_CELLULAR_CONFIG_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {
class Checkbox;
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

class NetworkConfigView;

// A dialog box for showing a password textfield.
class CellularConfigView : public views::View,
                           public views::ButtonListener,
                           public views::LinkController {
 public:
  CellularConfigView(NetworkConfigView* parent,
                     const CellularNetwork& cellular);
  explicit CellularConfigView(NetworkConfigView* parent);
  virtual ~CellularConfigView() {}

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* button, const views::Event& event);

  // views::LinkController implementation.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Save network information.
  virtual bool Save();

 private:

  // Initializes UI.
  void Init();

  // Updates UI.
  void Update();

  NetworkConfigView* parent_;

  CellularNetwork cellular_;

  views::Label* purchase_info_;
  views::NativeButton* purchase_more_button_;
  views::Label* remaining_data_info_;
  views::Label* expiration_info_;
  views::Checkbox* show_notification_checkbox_;
  views::Checkbox* autoconnect_checkbox_;
  views::Link* customer_support_link_;

  DISALLOW_COPY_AND_ASSIGN(CellularConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_CELLULAR_CONFIG_VIEW_H_
