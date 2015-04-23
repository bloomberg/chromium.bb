// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace proximity_auth {

class ProximityAuthService;

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler : public content::WebUIMessageHandler {
 public:
  ProximityAuthWebUIHandler();
  ~ProximityAuthWebUIHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthWebUIHandler);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
