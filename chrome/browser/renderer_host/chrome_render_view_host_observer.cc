// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"

#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/notification_service.h"
#include "content/common/url_constants.h"
#include "content/common/view_messages.h"

ChromeRenderViewHostObserver::ChromeRenderViewHostObserver(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
}

ChromeRenderViewHostObserver::~ChromeRenderViewHostObserver() {
}

void ChromeRenderViewHostObserver::Navigate(
    const ViewMsg_Navigate_Params& params) {
  const GURL& url = params.url;
  if (!render_view_host()->delegate()->IsExternalTabContainer() &&
      (url.SchemeIs(chrome::kHttpScheme) || url.SchemeIs(chrome::kHttpsScheme)))
    chrome_browser_net::PreconnectUrlAndSubresources(url);
}

bool ChromeRenderViewHostObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewHostObserver, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DomOperationResponse,
                        OnDomOperationResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderViewHostObserver::OnDomOperationResponse(
    const std::string& json_string, int automation_id) {
  DomOperationNotificationDetails details(json_string, automation_id);
  NotificationService::current()->Notify(
      NotificationType::DOM_OPERATION_RESPONSE,
      Source<RenderViewHost>(render_view_host()),
      Details<DomOperationNotificationDetails>(&details));
}
