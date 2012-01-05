// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NETWORK_ACTION_PREDICTOR_NETWORK_ACTION_PREDICTOR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NETWORK_ACTION_PREDICTOR_NETWORK_ACTION_PREDICTOR_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

class NetworkActionPredictorUI : public ChromeWebUI {
 public:
  explicit NetworkActionPredictorUI(content::WebContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkActionPredictorUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NETWORK_ACTION_PREDICTOR_NETWORK_ACTION_PREDICTOR_UI_H_
