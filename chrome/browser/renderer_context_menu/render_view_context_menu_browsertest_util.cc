// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
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

ContextMenuWaiter::ContextMenuWaiter(const content::NotificationSource& source)
    : menu_visible_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
                 content::NotificationService::AllSources());
}

ContextMenuWaiter::~ContextMenuWaiter() {
}

void ContextMenuWaiter::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN: {
      menu_visible_ = true;
      RenderViewContextMenu* context_menu =
          content::Source<RenderViewContextMenu>(source).ptr();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&ContextMenuWaiter::Cancel,
                                base::Unretained(this), context_menu));
      break;
    }

    default:
      NOTREACHED();
  }
}

void ContextMenuWaiter::WaitForMenuOpenAndClose() {
  content::WindowedNotificationObserver menu_observer(
      chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
      content::NotificationService::AllSources());
  if (!menu_visible_)
    menu_observer.Wait();

  content::RunAllPendingInMessageLoop();
  menu_visible_ = false;
}

content::ContextMenuParams& ContextMenuWaiter::params() {
  return params_;
}

void ContextMenuWaiter::Cancel(RenderViewContextMenu* context_menu) {
  params_ = context_menu->params();
  context_menu->Cancel();
}
