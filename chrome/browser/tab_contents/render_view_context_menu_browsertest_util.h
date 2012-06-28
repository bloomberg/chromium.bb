// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
#pragma once

#include "base/basictypes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

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

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
