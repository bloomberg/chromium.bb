// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIAGNOSTICS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIAGNOSTICS_UI_H_
#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// WebUI for end-user diagnostic information for Chrome OS devices, for users
// to self-diagnose hardware and software configuration problems and to help
// assist support personnel.
class DiagnosticsUI : public content::WebUIController {
 public:
  explicit DiagnosticsUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_DIAGNOSTICS_DIAGNOSTICS_UI_H_
