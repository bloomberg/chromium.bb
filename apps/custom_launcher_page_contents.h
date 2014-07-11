// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_CUSTOM_LAUNCHER_PAGE_CONTENTS_H_
#define APPS_CUSTOM_LAUNCHER_PAGE_CONTENTS_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace apps {

// Manages the web contents for extension-hosted launcher pages. The
// implementation for this class should create and maintain the WebContents for
// the page, and handle any message passing between the web contents and the
// extension system.
class CustomLauncherPageContents
    : public content::WebContentsObserver,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  CustomLauncherPageContents();
  virtual ~CustomLauncherPageContents();

  // Called to initialize and load the WebContents.
  void Initialize(content::BrowserContext* context, const GURL& url);

  content::WebContents* web_contents() const { return web_contents_.get(); }

 private:
  // content::WebContentsObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // extensions::ExtensionFunctionDispatcher::Delegate overrides:
  virtual extensions::WindowController* GetExtensionWindowController()
      const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(CustomLauncherPageContents);
};

}  // namespace apps

#endif  // APPS_CUSTOM_LAUNCHER_PAGE_CONTENTS_H_
