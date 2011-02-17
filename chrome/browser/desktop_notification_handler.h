// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_
#pragma once

#include "chrome/browser/tab_contents/tab_contents_observer.h"

struct ViewHostMsg_ShowNotification_Params;
class RenderProcessHost;

// Per-tab Desktop notification handler. Handles desktop notification IPCs
// coming in from the renderer.
class DesktopNotificationHandler {
 public:
  // tab_contents will be NULL when this object is used with non-tab contents,
  // i.e. ExtensionHost.
  DesktopNotificationHandler(TabContents* tab_contents,
                             RenderProcessHost* process);
  virtual ~DesktopNotificationHandler() {}

  bool OnMessageReceived(const IPC::Message& message);

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

// A wrapper around DesktopNotificationHandler that implements
// TabContentsObserver.
class DesktopNotificationHandlerForTC : public TabContentsObserver {
 public:
  // tab_contents will be NULL when this object is used with non-tab contents,
  // i.e. ExtensionHost.
  DesktopNotificationHandlerForTC(TabContents* tab_contents,
                                  RenderProcessHost* process);

  // TabContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  DesktopNotificationHandler handler_;
};

#endif  // CHROME_BROWSER_DESKTOP_NOTIFICATION_HANDLER_H_

