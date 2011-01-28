// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_
#pragma once

#include "chrome/browser/tab_contents/tab_contents_observer.h"
#include "ipc/ipc_message.h"

struct ViewHostMsg_ShowNotification_Params;
class RenderProcessHost;

// Per-tab Desktop notification handler. Handles desktop notification IPCs
// coming in from the renderer.
class DesktopNotificationHandler : public TabContentsObserver {
 public:
  explicit DesktopNotificationHandler(TabContents* tab_contents,
                                      RenderProcessHost* process);
  virtual ~DesktopNotificationHandler() {}

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  RenderProcessHost* GetRenderProcessHost();

 private:
  // IPC handlers.
  void OnShowDesktopNotification(
      const IPC::Message& message,
      const ViewHostMsg_ShowNotification_Params& params);
  void OnCancelDesktopNotification(const IPC::Message& message,
                                   int notification_id);
  void OnRequestNotificationPermission(const IPC::Message& message,
                                       const GURL& origin,
                                       int callback_id);

 private:
  TabContents* tab_;
  RenderProcessHost* process_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationHandler);
};

#endif  // CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_

