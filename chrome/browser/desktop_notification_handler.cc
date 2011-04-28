// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/desktop_notification_handler.h"

#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/desktop_notification_messages.h"

DesktopNotificationHandler::DesktopNotificationHandler(
    RenderViewHost* render_view_host)
  : RenderViewHostObserver(render_view_host) {
}

DesktopNotificationHandler::~DesktopNotificationHandler() {
}

bool DesktopNotificationHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DesktopNotificationHandler, message)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_Show, OnShow)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_Cancel, OnCancel)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_RequestPermission,
                        OnRequestPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DesktopNotificationHandler::OnShow(
    const DesktopNotificationHostMsg_Show_Params& params) {
  // Disallow HTML notifications from unwanted schemes.  javascript:
  // in particular allows unwanted cross-domain access.
  GURL url = params.contents_url;
  if (params.is_html &&
      !url.SchemeIs(chrome::kHttpScheme) &&
      !url.SchemeIs(chrome::kHttpsScheme) &&
      !url.SchemeIs(chrome::kExtensionScheme) &&
      !url.SchemeIs(chrome::kDataScheme)) {
    return;
  }

  RenderProcessHost* process = render_view_host()->process();
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(process->profile());

  service->ShowDesktopNotification(
    params,
    process->id(),
    routing_id(),
    DesktopNotificationService::PageNotification);
}

void DesktopNotificationHandler::OnCancel(int notification_id) {
  RenderProcessHost* process = render_view_host()->process();
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(process->profile());

  service->CancelDesktopNotification(
      process->id(),
      routing_id(),
      notification_id);
}

void DesktopNotificationHandler::OnRequestPermission(
    const GURL& source_origin, int callback_context) {
  if (render_view_host()->delegate()->RequestDesktopNotificationPermission(
          source_origin, callback_context)) {
    return;
  }

  RenderProcessHost* process = render_view_host()->process();
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(process->profile());
  service->RequestPermission(
      source_origin, process->id(), routing_id(), callback_context, NULL);
}
