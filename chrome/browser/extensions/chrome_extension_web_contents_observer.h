// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/common/stack_frame.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {

// An ExtensionWebContentsObserver that adds support for the extension error
// console, reloading crashed extensions and routing extension messages between
// renderers.
class ChromeExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<ChromeExtensionWebContentsObserver> {
 private:
  friend class content::WebContentsUserData<ChromeExtensionWebContentsObserver>;

  explicit ChromeExtensionWebContentsObserver(
      content::WebContents* web_contents);
  virtual ~ChromeExtensionWebContentsObserver();

  // content::WebContentsObserver overrides.
  virtual void RenderViewCreated(content::RenderViewHost* render_view_host)
      OVERRIDE;
  virtual bool OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) OVERRIDE;

  // Adds a message to the extensions ErrorConsole.
  void OnDetailedConsoleMessageAdded(const base::string16& message,
                                     const base::string16& source,
                                     const StackTrace& stack_trace,
                                     int32 severity_level);

  // Reloads an extension if it is on the terminated list.
  void ReloadIfTerminated(content::RenderViewHost* render_view_host);

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_WEB_CONTENTS_OBSERVER_H_
