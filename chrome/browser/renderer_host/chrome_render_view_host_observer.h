// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_VIEW_HOST_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_VIEW_HOST_OBSERVER_H_

#include "content/public/browser/render_view_host_observer.h"

class Profile;

namespace chrome_browser_net {
class Predictor;
}

namespace extensions {
class Extension;
}

// This class holds the Chrome specific parts of RenderViewHost, and has the
// same lifetime.
class ChromeRenderViewHostObserver : public content::RenderViewHostObserver {
 public:
  ChromeRenderViewHostObserver(content::RenderViewHost* render_view_host,
                               chrome_browser_net::Predictor* predictor);
  virtual ~ChromeRenderViewHostObserver();

  // content::RenderViewHostObserver overrides.
  virtual void RenderViewHostInitialized() OVERRIDE;
  virtual void RenderViewHostDestroyed(content::RenderViewHost* rvh) OVERRIDE;
  virtual void Navigate(const GURL& url) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
   // Does extension-specific initialization when a new renderer process is
   // created by a RenderViewHost.
  void InitRenderViewForExtensions();
  // Gets the extension or app (if any) that is associated with the RVH.
  const extensions::Extension* GetExtension();
  // Cleans up when a RenderViewHost is removed, or on destruction.
  void RemoveRenderViewHostForExtensions(content::RenderViewHost* rvh);
  void OnFocusedNodeTouched(bool editable);

  Profile* profile_;
  chrome_browser_net::Predictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderViewHostObserver);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_VIEW_HOST_OBSERVER_H_
