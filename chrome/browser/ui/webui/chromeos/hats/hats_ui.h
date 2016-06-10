// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_HATS_HATS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_HATS_HATS_UI_H_

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A WebUI to host HaTS survey.
class HatsUI : public ui::WebDialogUI {
 public:
  explicit HatsUI(content::WebUI* web_ui);
  ~HatsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HatsUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_HATS_HATS_UI_H_
