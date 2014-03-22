// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
class RenderViewHost;
class WebContents;
}

namespace extensions {
class Extension;

// A web contents observer used for renderer and extension processes. Grants the
// renderer access to certain URL scheme patterns for extensions and notifies
// the renderer that the extension was loaded.
//
// Extension system embedders must create an instance for every extension
// WebContents. It must be a subclass so that creating an instance via
// content::WebContentsUserData::CreateForWebContents() provides an object of
// the correct type. For an example, see ChromeExtensionWebContentsObserver.
class ExtensionWebContentsObserver : public content::WebContentsObserver {
 protected:
  explicit ExtensionWebContentsObserver(content::WebContents* web_contents);
  virtual ~ExtensionWebContentsObserver();

  content::BrowserContext* browser_context() { return browser_context_; }

  // content::WebContentsObserver overrides.

  // A subclass should invoke this method to finish extension process setup.
  virtual void RenderViewCreated(content::RenderViewHost* render_view_host)
      OVERRIDE;

  // Returns the extension or app associated with a render view host. Returns
  // NULL if the render view host is not for a valid extension.
  const Extension* GetExtension(content::RenderViewHost* render_view_host);

  // Updates ViewType for RenderViewHost based on GetViewType(web_contents()).
  void NotifyRenderViewType(content::RenderViewHost* render_view_host);

  // Returns the extension or app ID associated with a render view host. Returns
  // the empty string if the render view host is not for a valid extension.
  static std::string GetExtensionId(content::RenderViewHost* render_view_host);

 private:
  // The BrowserContext associated with the WebContents being observed.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_WEB_CONTENTS_OBSERVER_H_
