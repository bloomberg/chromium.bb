// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace settings {

// The base class handler of Javascript messages of settings pages.
class SettingsPageUIHandler : public content::WebUIMessageHandler {
 public:
  SettingsPageUIHandler();
  ~SettingsPageUIHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {}

 protected:
  // Helper method for responding to JS requests initiated with
  // cr.sendWithPromise(), for the case where the returned promise should be
  // resolved (request succeeded).
  void ResolveJavascriptCallback(const base::Value& callback_id,
                                 const base::Value& response);

  // Helper method for responding to JS requests initiated with
  // cr.sendWithPromise(), for the case where the returned promise should be
  // rejected (request failed).
  void RejectJavascriptCallback(const base::Value& callback_id,
                                const base::Value& response);

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsPageUIHandler);
};

// The WebUI handler for chrome://md-settings.
class MdSettingsUI : public content::WebUIController,
                     public content::WebContentsObserver {
 public:
  explicit MdSettingsUI(content::WebUI* web_ui);
  ~MdSettingsUI() override;

  // Overridden from content::WebContentsObserver:
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost *render_frame_host) override;
  void DocumentOnLoadCompletedInMainFrame() override;

 private:
  void AddSettingsPageUIHandler(content::WebUIMessageHandler* handler);

  base::Time load_start_time_;

  DISALLOW_COPY_AND_ASSIGN(MdSettingsUI);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_
