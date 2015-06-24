// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"

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
class ExtensionWebContentsObserver
    : public content::WebContentsObserver,
      public ExtensionFunctionDispatcher::Delegate {
 public:
  // Returns the ExtensionWebContentsObserver for the given |web_contents|.
  static ExtensionWebContentsObserver* GetForWebContents(
      content::WebContents* web_contents);

  ExtensionFunctionDispatcher* dispatcher() { return &dispatcher_; }

 protected:
  explicit ExtensionWebContentsObserver(content::WebContents* web_contents);
  ~ExtensionWebContentsObserver() override;

  content::BrowserContext* browser_context() { return browser_context_; }

  // Initializes a new render frame. Subclasses should invoke this
  // implementation if extending.
  virtual void InitializeRenderFrame(
      content::RenderFrameHost* render_frame_host);

  // ExtensionFunctionDispatcher::Delegate overrides.
  content::WebContents* GetAssociatedWebContents() const override;

  // content::WebContentsObserver overrides.

  // A subclass should invoke this method to finish extension process setup.
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // Subclasses should call this first before doing their own message handling.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // Per the documentation in WebContentsObserver, these two methods are invoked
  // when a Pepper plugin instance is attached/detached in the page DOM.
  void PepperInstanceCreated() override;
  void PepperInstanceDeleted() override;

  // Returns the extension id associated with the given |render_frame_host|, or
  // the empty string if there is none.
  std::string GetExtensionIdFromFrame(
      content::RenderFrameHost* render_frame_host) const;

  // Returns the extension associated with the given |render_frame_host|, or
  // null if there is none.
  const Extension* GetExtensionFromFrame(
      content::RenderFrameHost* render_frame_host) const;

  // TODO(devlin): Remove these once callers are updated to use the FromFrame
  // equivalents.
  // Returns the extension or app associated with a render view host. Returns
  // NULL if the render view host is not for a valid extension.
  const Extension* GetExtension(content::RenderViewHost* render_view_host);
  // Returns the extension or app ID associated with a render view host. Returns
  // the empty string if the render view host is not for a valid extension.
  static std::string GetExtensionId(content::RenderViewHost* render_view_host);

 private:
  void OnRequest(content::RenderFrameHost* render_frame_host,
                 const ExtensionHostMsg_Request_Params& params);

  // A helper function for initializing render frames at the creation of the
  // observer.
  void InitializeFrameHelper(content::RenderFrameHost* render_frame_host);

  // The BrowserContext associated with the WebContents being observed.
  content::BrowserContext* browser_context_;

  ExtensionFunctionDispatcher dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_WEB_CONTENTS_OBSERVER_H_
