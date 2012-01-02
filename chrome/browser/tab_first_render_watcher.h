// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_FIRST_RENDER_WATCHER_H_
#define CHROME_BROWSER_TAB_FIRST_RENDER_WATCHER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class RenderViewHost;

namespace content {
class WebContents;
}

// This class watches given TabContent's loading and rendering state change.
class TabFirstRenderWatcher : public content::NotificationObserver {
 public:
  class Delegate {
   public:
    virtual void OnRenderHostCreated(RenderViewHost* host) = 0;
    virtual void OnTabMainFrameLoaded() = 0;
    virtual void OnTabMainFrameFirstRender() = 0;
  };

  TabFirstRenderWatcher(content::WebContents* tab, Delegate* delegate);

 private:
  // Overridden from content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  enum State {
    NONE,
    LOADED,        // Renderer loaded the page.
    FIRST_PAINT,   // 1st paint event after the page is loaded.
  };
  State state_;

  // WebContents that this class watches.
  content::WebContents* web_contents_;

  // Delegate to notify.
  Delegate* delegate_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabFirstRenderWatcher);
};

#endif  // CHROME_BROWSER_TAB_FIRST_RENDER_WATCHER_H_
