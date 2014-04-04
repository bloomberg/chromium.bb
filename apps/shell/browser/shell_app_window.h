// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_APP_WINDOW_H_
#define APPS_SHELL_BROWSER_SHELL_APP_WINDOW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"

struct ExtensionHostMsg_Request_Params;
class GURL;

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class ExtensionFunctionDispatcher;
}

namespace gfx {
class Size;
}

namespace apps {

// A simplified app window created by chrome.app.window.create(). Manages the
// primary web contents for the app.
class ShellAppWindow
    : public content::WebContentsObserver,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  ShellAppWindow();
  virtual ~ShellAppWindow();

  // Creates the web contents and attaches extension-specific helpers.
  // Passing a valid |initial_size| to avoid a web contents resize.
  void Init(content::BrowserContext* context, gfx::Size initial_size);

  // Starts loading |url| which must be an extension URL.
  void LoadURL(const GURL& url);

  // Returns the window hosting the web contents.
  aura::Window* GetNativeWindow();

  // Returns the routing ID of the render view host of |web_contents_|.
  int GetRenderViewRoutingID();

  // content::WebContentsObserver implementation
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // extensions::ExtensionFunctionDispatcher::Delegate implementation
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

 private:
  // IPC handler.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ShellAppWindow);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_APP_WINDOW_H_
