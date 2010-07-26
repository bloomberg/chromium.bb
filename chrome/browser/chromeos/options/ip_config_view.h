// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_IP_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_IP_CONFIG_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/view.h"

namespace views {
class Textfield;
}

namespace chromeos {

// A dialog box for showing a password textfield.
class IPConfigView : public views::View {
 public:
  explicit IPConfigView(const std::string& device_path);
  virtual ~IPConfigView() {}

 private:
  // Refreshes IP Config data.
  void RefreshData();
  // Initializes UI.
  void Init();

  // The device path of the network.
  std::string device_path_;

  NetworkIPConfigVector ip_configs_;

  views::Textfield* address_textfield_;
  views::Textfield* netmask_textfield_;
  views::Textfield* gateway_textfield_;
  views::Textfield* dnsserver_textfield_;

  DISALLOW_COPY_AND_ASSIGN(IPConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_IP_CONFIG_VIEW_H_
