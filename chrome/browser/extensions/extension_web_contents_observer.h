// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/common/stack_frame.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
struct Message;

// A web contents observer that's used for WebContents in renderer and extension
// processes. Grants the renderer access to certain URL patterns for extensions,
// notifies the renderer that the extension was loaded and routes messages to
// the MessageService.
class ExtensionWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ExtensionWebContentsObserver> {
 public:
  virtual ~ExtensionWebContentsObserver();

 private:
  explicit ExtensionWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ExtensionWebContentsObserver>;

  // content::WebContentsObserver overrides.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnPostMessage(int port_id, const Message& message);
  void OnDetailedConsoleMessageAdded(const base::string16& message,
                                     const base::string16& source,
                                     const StackTrace& stack_trace,
                                     int32 severity_level);

  // Gets the extension or app (if any) that is associated with a RVH. If
  // |reload_if_terminated| is true then terminated extensions will be reloaded.
  const Extension* GetExtension(content::RenderViewHost* render_view_host,
                                bool reload_if_terminated);

  // The browser context for the web contents this is observing.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
