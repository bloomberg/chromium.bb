// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_GCM_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_GCM_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://gcm-internals.
class GCMInternalsUI : public content::WebUIController {
 public:
  explicit GCMInternalsUI(content::WebUI* web_ui);
  ~GCMInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_GCM_INTERNALS_UI_H_
