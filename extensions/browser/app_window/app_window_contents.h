// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CONTENTS_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CONTENTS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {

struct DraggableRegion;

// AppWindowContents class specific to app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class AppWindowContentsImpl : public AppWindowContents,
                              public content::WebContentsObserver,
                              public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit AppWindowContentsImpl(AppWindow* host);
  virtual ~AppWindowContentsImpl();

  // AppWindowContents
  virtual void Initialize(content::BrowserContext* context,
                          const GURL& url) OVERRIDE;
  virtual void LoadContents(int32 creator_process_id) OVERRIDE;
  virtual void NativeWindowChanged(NativeAppWindow* native_app_window) OVERRIDE;
  virtual void NativeWindowClosed() OVERRIDE;
  virtual void DispatchWindowShownForTests() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  // content::WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate
  virtual WindowController* GetExtensionWindowController() const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void UpdateDraggableRegions(const std::vector<DraggableRegion>& regions);
  void SuspendRenderViewHost(content::RenderViewHost* rvh);

  AppWindow* host_;  // This class is owned by |host_|
  GURL url_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowContentsImpl);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WINDOW_CONTENTS_H_
