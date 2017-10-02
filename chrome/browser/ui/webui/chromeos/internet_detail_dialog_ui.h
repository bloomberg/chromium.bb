// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_DETAIL_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_DETAIL_DIALOG_UI_H_

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A WebUI to host a subset of the network details page to allow setting of
// proxy, IP address, and nameservers in the login screen.
class InternetDetailDialogUI : public ui::WebDialogUI {
 public:
  explicit InternetDetailDialogUI(content::WebUI* web_ui);
  ~InternetDetailDialogUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternetDetailDialogUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INTERNET_DETAIL_DIALOG_UI_H_
