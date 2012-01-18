// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_INTERNALS_UI_H_
#pragma once

#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://media-internals page.
class MediaInternalsUI : public content::WebUIController {
 public:
  explicit MediaInternalsUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_INTERNALS_UI_H_
