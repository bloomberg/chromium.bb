// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace safe_browsing {
class SafeBrowsingUIHandler : public content::WebUIMessageHandler {
 public:
  SafeBrowsingUIHandler();
  ~SafeBrowsingUIHandler() override;
  void ExpParamList(const base::ListValue* args);
  void RegisterMessages() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIHandler);
};

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public content::WebUIController {
 public:
  explicit SafeBrowsingUI(content::WebUI* web_ui);
  ~SafeBrowsingUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUI);
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_WEBUI_SAFE_BROWSING_UI_H_
