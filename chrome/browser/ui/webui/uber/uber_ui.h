// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
#pragma once

#include <string>

#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

// The ChromeWebUI class for the uber page (chrome://chrome). It manages the
// UI for the uber page (navigation bar and so forth) as well as ChromeWebUI
// objects for pages that appear in the uber page.
class UberUI : public ChromeWebUI {
 public:
  explicit UberUI(TabContents* contents);
  virtual ~UberUI();

  // ChromeWebUI implementation.
  virtual void OnWebUISend(const GURL& source_url,
                           const std::string& message,
                           const ListValue& args) OVERRIDE;

  // We forward these to |sub_uis_|.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReused(RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidBecomeActiveForReusedRenderView() OVERRIDE;

 private:
  // Collection of WebUIs for the subpages within uber.
  ScopedVector<ChromeWebUI> sub_uis_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
