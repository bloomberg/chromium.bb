// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"

class Profile;

class DevToolsUI : public content::WebUIController,
                   public content::WebContentsObserver {
 public:
  static GURL GetProxyURL(const std::string& frontend_url);

  explicit DevToolsUI(content::WebUI* web_ui);
  virtual ~DevToolsUI();

  // content::WebContentsObserver overrides.
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;

 private:
  void RemotePageOpened(const GURL& virtual_url,
                        DevToolsAndroidBridge::RemotePage* page);

  DevToolsUIBindings bindings_;
  base::WeakPtrFactory<DevToolsUI> weak_factory_;
  GURL remote_frontend_loading_url_;
  GURL remote_page_opening_url_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
