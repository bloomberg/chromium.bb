// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/restore_tab_helper.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

RestoreTabHelper::RestoreTabHelper(TabContentsWrapper* tab)
    : TabContentsObserver(tab->tab_contents()),
      tab_(tab) {
}

RestoreTabHelper::~RestoreTabHelper() {
}

void RestoreTabHelper::SetWindowID(const SessionID& id) {
  window_id_ = id;
  NotificationService::current()->Notify(
      NotificationType::TAB_PARENTED,
      Source<TabContentsWrapper>(tab_),
      NotificationService::NoDetails());

  // Extension code in the renderer holds the ID of the window that hosts it.
  // Notify it that the window ID changed.
  tab_->render_view_host()->Send(new ExtensionMsg_UpdateBrowserWindowId(
      tab_->render_view_host()->routing_id(), id.id()));
}

void RestoreTabHelper::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(
      new ExtensionMsg_UpdateBrowserWindowId(render_view_host->routing_id(),
                                             window_id_.id()));
}
