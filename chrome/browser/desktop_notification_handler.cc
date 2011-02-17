// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/desktop_notification_handler.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

DesktopNotificationHandler::DesktopNotificationHandler(
    TabContents* tab, RenderProcessHost* process)
  : tab_(tab),
    process_(process) {
}

bool DesktopNotificationHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DesktopNotificationHandler, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowDesktopNotification,
                        OnShowDesktopNotification)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelDesktopNotification,
                        OnCancelDesktopNotification)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestNotificationPermission,
                        OnRequestNotificationPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DesktopNotificationHandler::OnShowDesktopNotification(
    const IPC::Message& message,
    const ViewHostMsg_ShowNotification_Params& params) {
  RenderProcessHost* process = GetRenderProcessHost();
  DesktopNotificationService* service =
      process->profile()->GetDesktopNotificationService();

  service->ShowDesktopNotification(
    params,
    process->id(),
    message.routing_id(),
    DesktopNotificationService::PageNotification);
}

void DesktopNotificationHandler::OnCancelDesktopNotification(
    const IPC::Message& message, int notification_id) {
  RenderProcessHost* process = GetRenderProcessHost();
  DesktopNotificationService* service =
      process->profile()->GetDesktopNotificationService();

  service->CancelDesktopNotification(
      process->id(),
      message.routing_id(),
      notification_id);
}

void DesktopNotificationHandler::OnRequestNotificationPermission(
    const IPC::Message& message, const GURL& source_origin,
    int callback_context) {
  RenderProcessHost* process = GetRenderProcessHost();
  Browser* browser = BrowserList::GetLastActive();
  // We may not have a BrowserList if the chrome browser process is launched as
  // a ChromeFrame process in which case we attempt to use the TabContents
  // provided by the RenderViewHostDelegate.
  TabContents* tab = browser ? browser->GetSelectedTabContents() : tab_;
  if (!tab)
    return;

  DesktopNotificationService* service =
      tab->profile()->GetDesktopNotificationService();
  service->RequestPermission(
      source_origin,
      process->id(),
      message.routing_id(),
      callback_context,
      tab);
}

RenderProcessHost* DesktopNotificationHandler::GetRenderProcessHost() {
  return tab_ ? tab_->GetRenderProcessHost() : process_;
}

DesktopNotificationHandlerForTC::DesktopNotificationHandlerForTC(
    TabContents* tab_contents,
    RenderProcessHost* process)
    : TabContentsObserver(tab_contents),
      DesktopNotificationHandler(tab_contents, process) {
}
