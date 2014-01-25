// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NFC_DEBUG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NFC_DEBUG_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

class NfcDebugUI : public content::WebUIController {
 public:
  explicit NfcDebugUI(content::WebUI* web_ui);
  virtual ~NfcDebugUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcDebugUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NFC_DEBUG_UI_H_
