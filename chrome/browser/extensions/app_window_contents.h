// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_WINDOW_CONTENTS_H_
#define CHROME_BROWSER_EXTENSIONS_APP_WINDOW_CONTENTS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class RenderViewHost;
}

namespace extensions {
struct DraggableRegion;
}

// ShellWindowContents class specific to app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class AppWindowContents : public ShellWindowContents,
                          public content::NotificationObserver,
                          public content::WebContentsObserver,
                          public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit AppWindowContents(ShellWindow* host);
  virtual ~AppWindowContents();

  // ShellWindowContents
  virtual void Initialize(Profile* profile, const GURL& url) OVERRIDE;
  virtual void LoadContents(int32 creator_process_id) OVERRIDE;
  virtual void NativeWindowChanged(NativeAppWindow* native_app_window) OVERRIDE;
  virtual void NativeWindowClosed() OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  void OnRequest(const ExtensionHostMsg_Request_Params& params);
  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions);
  void SuspendRenderViewHost(content::RenderViewHost* rvh);

  ShellWindow* host_;  // This class is owned by |host_|
  GURL url_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowContents);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_WINDOW_CONTENTS_H_
