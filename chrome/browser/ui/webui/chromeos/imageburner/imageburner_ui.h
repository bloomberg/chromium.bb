// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_IMAGEBURNER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_IMAGEBURNER_UI_H_

#include "content/public/browser/web_ui_controller.h"

class ImageBurnUI : public content::WebUIController {
 public:
  explicit ImageBurnUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageBurnUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IMAGEBURNER_IMAGEBURNER_UI_H_

