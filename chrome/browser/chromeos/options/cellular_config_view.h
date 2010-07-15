// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_CELLULAR_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_CELLULAR_CONFIG_VIEW_H_

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "views/controls/button/checkbox.h"
#include "views/view.h"

namespace chromeos {

class NetworkConfigView;

// A dialog box for showing a password textfield.
class CellularConfigView : public views::View {
 public:
  CellularConfigView(NetworkConfigView* parent,
                     const CellularNetwork& cellular);
  explicit CellularConfigView(NetworkConfigView* parent);
  virtual ~CellularConfigView() {}

  // Save network information.
  virtual bool Save();

 private:

  // Initializes UI.
  void Init();

  NetworkConfigView* parent_;

  CellularNetwork cellular_;

  views::Checkbox* autoconnect_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(CellularConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_CELLULAR_CONFIG_VIEW_H_
