// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_H_

#include <string>

#include "content/public/browser/web_ui_controller.h"

namespace proximity_auth {

class ProximityAuthClient;

// The WebUI controller for chrome://proximity-auth.
class ProximityAuthUI : public content::WebUIController {
 public:
  // Note: |web_ui| and |delegate| are not owned by this instance and must
  // outlive this instance.
  ProximityAuthUI(content::WebUI* web_ui, ProximityAuthClient* delegate);
  ~ProximityAuthUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthUI);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_H_
