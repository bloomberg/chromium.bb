// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://webrtc-internals page.
class WebRTCInternalsUI : public content::WebUIController {
 public:
  explicit WebRTCInternalsUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRTCInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_INTERNALS_UI_H_
