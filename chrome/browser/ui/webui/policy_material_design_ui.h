// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_MATERIAL_DESIGN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_MATERIAL_DESIGN_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// The Web UI controller for the chrome://md-policy page.
class PolicyMaterialDesignUI : public content::WebUIController {
 public:
  explicit PolicyMaterialDesignUI(content::WebUI* web_ui);
  ~PolicyMaterialDesignUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyMaterialDesignUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_MATERIAL_DESIGN_UI_H_
