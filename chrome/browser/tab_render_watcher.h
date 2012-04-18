// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_RENDER_WATCHER_H_
#define CHROME_BROWSER_TAB_RENDER_WATCHER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class RenderViewHost;
class WebContents;
}

// This class watches given WebContents's loading and rendering state change.
class TabRenderWatcher : public content::NotificationObserver {
 public:
  class Delegate {
   public:
    virtual void OnRenderHostCreated(content::RenderViewHost* host) = 0;
    virtual void OnTabMainFrameLoaded() = 0;
    virtual void OnTabMainFrameRender() = 0;
  };

  TabRenderWatcher(content::WebContents* tab, Delegate* delegate);

 private:
  // Overridden from content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Has the renderer loaded the page yet?
  bool loaded_;

  // WebContents that this class watches.
  content::WebContents* web_contents_;

  // Delegate to notify.
  Delegate* delegate_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabRenderWatcher);
};

#endif  // CHROME_BROWSER_TAB_RENDER_WATCHER_H_
