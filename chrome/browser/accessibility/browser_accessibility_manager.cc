// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_accessibility_manager.h"

#include "chrome/common/render_messages_params.h"

using webkit_glue::WebAccessibility;


BrowserAccessibilityManager::BrowserAccessibilityManager(
    gfx::NativeWindow parent_window)
    : parent_window_(parent_window) {
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
}

void BrowserAccessibilityManager::OnAccessibilityNotifications(
    const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params) {
  for (uint32 index = 0; index < params.size(); index++) {
    const ViewHostMsg_AccessibilityNotification_Params& param = params[index];

    switch (param.notification_type) {
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHECK_STATE_CHANGED:
        OnAccessibilityObjectStateChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_CHILDREN_CHANGED:
        OnAccessibilityObjectChildrenChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_FOCUS_CHANGED:
        OnAccessibilityObjectFocusChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_LOAD_COMPLETE:
        OnAccessibilityObjectLoadComplete(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_VALUE_CHANGED:
        OnAccessibilityObjectValueChange(param.acc_obj);
        break;
      case ViewHostMsg_AccessibilityNotification_Params::
          NOTIFICATION_TYPE_SELECTED_TEXT_CHANGED:
        OnAccessibilityObjectTextChange(param.acc_obj);
        break;
      default:
        DCHECK(0);
        break;
    }
  }
}

gfx::NativeWindow BrowserAccessibilityManager::GetParentWindow() {
  return parent_window_;
}
