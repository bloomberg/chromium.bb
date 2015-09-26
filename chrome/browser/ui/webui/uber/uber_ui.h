// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistry;
}

// Logs visits to subframe URLs (e.g. chrome://settings-frame).
class SubframeLogger : public content::WebContentsObserver {
 public:
  explicit SubframeLogger(content::WebContents* contents);
  ~SubframeLogger() override;

  // content::WebContentsObserver implementation.
  void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;

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
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewReused(content::RenderViewHost* render_view_host) override;

 private:
  // A map from URL origin to WebUI instance.
  typedef std::map<std::string, content::WebUI*> SubpageMap;

  // Creates and stores a WebUI for the given URL.
  void RegisterSubpage(const std::string& page_url,
                       const std::string& page_host);

  SubframeLogger subframe_logger_;

  // The WebUI*s in this map are owned.
  SubpageMap sub_uis_;

  DISALLOW_COPY_AND_ASSIGN(UberUI);
};

class UberFrameUI : public content::WebUIController,
                    public extensions::ExtensionRegistryObserver {
 public:
  explicit UberFrameUI(content::WebUI* web_ui);
  ~UberFrameUI() override;

 private:
  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(UberFrameUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_UBER_UBER_UI_H_
