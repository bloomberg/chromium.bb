// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_WINDOW_CONTENTS_H_
#define APPS_APP_WINDOW_CONTENTS_H_

#include "apps/app_window.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {
struct DraggableRegion;
}

namespace apps {

// AppWindowContents class specific to app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class AppWindowContentsImpl
    : public AppWindowContents,
      public content::WebContentsObserver,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  explicit AppWindowContentsImpl(AppWindow* host);
  virtual ~AppWindowContentsImpl();

  // AppWindowContents
  virtual void Initialize(content::BrowserContext* context,
                          const GURL& url) OVERRIDE;
  virtual void LoadContents(int32 creator_process_id) OVERRIDE;
  virtual void NativeWindowChanged(
      extensions::NativeAppWindow* native_app_window) OVERRIDE;
  virtual void NativeWindowClosed() OVERRIDE;
  virtual void DispatchWindowShownForTests() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  // content::WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // extensions::ExtensionFunctionDispatcher::Delegate
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions);
  void SuspendRenderViewHost(content::RenderViewHost* rvh);

  AppWindow* host_;  // This class is owned by |host_|
  GURL url_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<extensions::ExtensionFunctionDispatcher>
      extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowContentsImpl);
};

}  // namespace apps

#endif  // APPS_APP_WINDOW_CONTENTS_H_
