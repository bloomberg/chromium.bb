// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_TOOL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_TOOL_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The UI for chrome://cleanup, which will allow Windows users to see the
// status of the last Chrome Cleanup Tool scan and to manually launch the
// cleanup process.
class CleanupToolUI : public content::WebUIController {
 public:
  explicit CleanupToolUI(content::WebUI* web_ui);
  ~CleanupToolUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CleanupToolUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CLEANUP_TOOL_CLEANUP_TOOL_UI_H_
