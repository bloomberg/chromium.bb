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
  virtual ~ContextMenuNotificationObserver();

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void ExecuteCommand(RenderViewContextMenu* context_menu);

  content::NotificationRegistrar registrar_;
  int command_to_execute_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuNotificationObserver);
};

class SaveLinkAsContextMenuObserver : public ContextMenuNotificationObserver {
 public:
  // Register to listen for notifications of
  // NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN from either
  // a specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  explicit SaveLinkAsContextMenuObserver(
      const content::NotificationSource& source);
  virtual ~SaveLinkAsContextMenuObserver();

  // Suggested filename for file downloaded through "Save Link As" option.
  base::string16 GetSuggestedFilename();

  // Wait for context menu to be visible.
  void WaitForMenu();

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void Cancel(RenderViewContextMenu* context_menu);

  bool menu_visible_;
  content::ContextMenuParams params_;

  DISALLOW_COPY_AND_ASSIGN(SaveLinkAsContextMenuObserver);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
