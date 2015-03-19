// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/context_menu_params.h"

class RenderViewContextMenu;

class ContextMenuNotificationObserver : public content::NotificationObserver {
 public:
  // Wait for a context menu to be shown, and then execute |command_to_execute|.
  explicit ContextMenuNotificationObserver(int command_to_execute);
  ~ContextMenuNotificationObserver() override;

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void ExecuteCommand(RenderViewContextMenu* context_menu);

  content::NotificationRegistrar registrar_;
  int command_to_execute_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuNotificationObserver);
};

class ContextMenuWaiter : public content::NotificationObserver {
 public:
  // Register to listen for notifications of
  // NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN from either
  // a specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  explicit ContextMenuWaiter(const content::NotificationSource& source);
  ~ContextMenuWaiter() override;

  content::ContextMenuParams& params();

  // Wait until the context menu is opened and closed.
  void WaitForMenuOpenAndClose();

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void Cancel(RenderViewContextMenu* context_menu);

  bool menu_visible_;
  content::ContextMenuParams params_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuWaiter);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
