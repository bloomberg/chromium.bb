// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/athena/athena_util.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"

content::WebContents* GetWebContentsForWindow(aura::Window* owner_window) {
  if (!owner_window) {
    athena::WindowListProvider* window_list =
        athena::WindowManager::Get()->GetWindowListProvider();
    DCHECK(window_list->GetWindowList().size());
    owner_window = window_list->GetWindowList().back();
  }
  athena::Activity* activity =
      athena::ActivityManager::Get()->GetActivityForWindow(owner_window);
  return activity->GetWebContents();
}
