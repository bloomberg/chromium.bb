// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MD_HISTORY_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

class Profile;

namespace base {
class RefCountedMemory;
}

class MdHistoryUI : public content::WebUIController {
 public:
  explicit MdHistoryUI(content::WebUI* web_ui);
  ~MdHistoryUI() override;

  static bool IsEnabled(Profile* profile);
  // Reset the current list of features and explicitly disable MD History.
  static void DisableForTesting();

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(MdHistoryUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_HISTORY_UI_H_
