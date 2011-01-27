// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_
#pragma once

#include "chrome/browser/tab_contents/web_navigation_observer.h"

struct ViewHostMsg_ShowNotification_Params;
class RenderProcessHost;

// Per-tab Desktop notification handler. Handles desktop notification IPCs
// coming in from the renderer.
class DesktopNotificationHandler : public WebNavigationObserver {
 public:
  explicit DesktopNotificationHandler(TabContents* tab_contents,
                                      RenderProcessHost* process);
  virtual ~DesktopNotificationHandler() {}

  // WebNavigationObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  RenderProcessHost* GetRenderProcessHost();

 private:
  // IPC handlers.
  void OnShowDesktopNotification(
      const ViewHostMsg_ShowNotification_Params& params);
  void OnCancelDesktopNotification(int routing_id, int notification_id);
  void OnRequestNotificationPermission(int routing_id, const GURL& origin,
                                       int callback_id);

 private:
  TabContents* tab_;
  RenderProcessHost* process_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationHandler);
};

#endif  // CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_

