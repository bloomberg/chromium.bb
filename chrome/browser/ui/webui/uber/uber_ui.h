// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"

// Logs visits to subframe URLs (e.g. chrome://settings-frame).
class SubframeLogger : public content::WebContentsObserver {
 public:
  explicit SubframeLogger(content::WebContents* contents);
  ~SubframeLogger() override;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SubframeLogger);
};

// The WebUI class for the uber page (chrome://chrome). It manages the UI for
// the uber page (navigation bar and so forth) as well as WebUI objects for
// pages that appear in the uber page.
class UberUI : public content::WebUIController {
 public:
  explicit UberUI(content::WebUI* web_ui);
  ~UberUI() override;

  content::WebUI* GetSubpage(const std::string& page_url);

  // WebUIController implementation.
  bool OverrideHandleWebUIMessage(const GURL& source_url,
                                  const std::string& message,
                                  const base::ListValue& args) override;

  // We forward these to |sub_uis_|.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

 private:
  // Creates and stores a WebUI for the given URL.
  void RegisterSubpage(const std::string& page_url,
                       const std::string& page_host);

  std::unique_ptr<SubframeLogger> subframe_logger_;

  // Map from URL origin to WebUI instance.
  std::map<std::string, std::unique_ptr<content::WebUI>> sub_uis_;

  DISALLOW_COPY_AND_ASSIGN(UberUI);
};

class UberFrameUI : public content::WebUIController {
 public:
  explicit UberFrameUI(content::WebUI* web_ui);
  ~UberFrameUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
