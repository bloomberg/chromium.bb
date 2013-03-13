// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_browsertest_util.h"

#include "base/message_loop.h"
#include "base/bind.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

ContextMenuNotificationObserver::ContextMenuNotificationObserver(
    int command_to_execute)
    : command_to_execute_(command_to_execute) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
                 content::NotificationService::AllSources());
}

ContextMenuNotificationObserver::~ContextMenuNotificationObserver() {
}

void ContextMenuNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN: {
      RenderViewContextMenu* context_menu =
          content::Source<RenderViewContextMenu>(source).ptr();
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ContextMenuNotificationObserver::ExecuteCommand,
                     base::Unretained(this), context_menu));
      break;
    }

    default:
      NOTREACHED();
  }
}

void ContextMenuNotificationObserver::ExecuteCommand(
    RenderViewContextMenu* context_menu) {
  context_menu->ExecuteCommand(command_to_execute_, 0);
  context_menu->Cancel();
}
