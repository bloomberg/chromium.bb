// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <oleacc.h>
#endif

#include <vector>

#include "gfx/native_widget_types.h"
#include "webkit/glue/webaccessibility.h"

struct ViewHostMsg_AccessibilityNotification_Params;

using webkit_glue::WebAccessibility;

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
class BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}
  virtual void SetAccessibilityFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
};

// Manages a tree of BrowserAccessibility objects.
class BrowserAccessibilityManager {
 public:
  // Creates the platform specific BrowserAccessibilityManager. Ownership passes
  // to the caller.
  static BrowserAccessibilityManager* Create(
    gfx::NativeWindow parent_window,
    const webkit_glue::WebAccessibility& src,
    BrowserAccessibilityDelegate* delegate);

  virtual ~BrowserAccessibilityManager();

  // Called when the renderer process has notified us of about tree changes.
  // Send a notification to MSAA clients of the change.
  void OnAccessibilityNotifications(
    const std::vector<ViewHostMsg_AccessibilityNotification_Params>& params);

  gfx::NativeWindow GetParentWindow();

#if defined(OS_WIN)
  virtual IAccessible* GetRootAccessible() = 0;
#endif

 protected:
  explicit BrowserAccessibilityManager(gfx::NativeWindow parent_window);

  virtual void OnAccessibilityObjectStateChange(
      const webkit_glue::WebAccessibility& acc_obj) = 0;
  virtual void OnAccessibilityObjectChildrenChange(
      const webkit_glue::WebAccessibility& acc_obj) = 0;
  virtual void OnAccessibilityObjectFocusChange(
      const webkit_glue::WebAccessibility& acc_obj) = 0;
  virtual void OnAccessibilityObjectLoadComplete(
      const webkit_glue::WebAccessibility& acc_obj) = 0;
  virtual void OnAccessibilityObjectValueChange(
      const webkit_glue::WebAccessibility& acc_obj) = 0;
  virtual void OnAccessibilityObjectTextChange(
      const webkit_glue::WebAccessibility& acc_obj) = 0;

 private:
  // The parent window.
  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
