// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/window_proxy.h"

#include <vector>
#include <algorithm>

#include "base/logging.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

bool WindowProxy::SimulateOSClick(const gfx::Point& click, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(new AutomationMsg_WindowClick(handle_, click, flags));
}

bool WindowProxy::SimulateOSMouseMove(const gfx::Point& location) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowMouseMove(handle_, location));
}

bool WindowProxy::GetWindowTitle(string16* text) {
  if (!is_valid()) return false;

  if (!text) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_WindowTitle(handle_, text));
}

bool WindowProxy::SimulateOSKeyPress(ui::KeyboardCode key, int flags) {
  if (!is_valid()) return false;

  return sender_->Send(
      new AutomationMsg_WindowKeyPress(handle_, key, flags));
}

bool WindowProxy::SetVisible(bool visible) {
  if (!is_valid()) return false;

  bool result = false;

  sender_->Send(new AutomationMsg_SetWindowVisible(handle_, visible, &result));
  return result;
}

bool WindowProxy::IsActive(bool* active) {
  if (!is_valid()) return false;

  bool result = false;

  sender_->Send(new AutomationMsg_IsWindowActive(handle_, &result, active));
  return result;
}

bool WindowProxy::Activate() {
  if (!is_valid()) return false;

  return sender_->Send(new AutomationMsg_ActivateWindow(handle_));
}

bool WindowProxy::GetViewBounds(int view_id, gfx::Rect* bounds,
                                bool screen_coordinates) {
  if (!is_valid())
    return false;

  if (!bounds) {
    NOTREACHED();
    return false;
  }

  bool result = false;

  if (!sender_->Send(new AutomationMsg_WindowViewBounds(
          handle_, view_id, screen_coordinates, &result, bounds))) {
    return false;
  }

  return result;
}

bool WindowProxy::GetBounds(gfx::Rect* bounds) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_GetWindowBounds(handle_, bounds, &result));
  return result;
}

bool WindowProxy::SetBounds(const gfx::Rect& bounds) {
  if (!is_valid())
    return false;
  bool result = false;
  sender_->Send(new AutomationMsg_SetWindowBounds(handle_, bounds, &result));
  return result;
}

bool WindowProxy::GetFocusedViewID(int* view_id) {
  if (!is_valid()) return false;

  if (!view_id) {
    NOTREACHED();
    return false;
  }

  return sender_->Send(new AutomationMsg_GetFocusedViewID(handle_, view_id));
}

bool WindowProxy::WaitForFocusedViewIDToChange(
    int old_view_id, int* new_view_id) {
  bool result = false;
  if (!sender_->Send(new AutomationMsg_WaitForFocusedViewIDToChange(
                     handle_, old_view_id, &result, new_view_id)))
    return false;
  return result;
}

scoped_refptr<BrowserProxy> WindowProxy::GetBrowser() {
  return GetBrowserWithTimeout(base::kNoTimeout, NULL);
}

scoped_refptr<BrowserProxy> WindowProxy::GetBrowserWithTimeout(
    uint32 timeout_ms, bool* is_timeout) {
  if (!is_valid())
    return NULL;

  bool handle_ok = false;
  int browser_handle = 0;

  sender_->Send(new AutomationMsg_BrowserForWindow(handle_, &handle_ok,
                                                   &browser_handle));
  if (!handle_ok)
    return NULL;

  BrowserProxy* browser =
      static_cast<BrowserProxy*>(tracker_->GetResource(browser_handle));
  if (!browser) {
    browser = new BrowserProxy(sender_, tracker_, browser_handle);
    browser->AddRef();
  }

  // Since there is no scoped_refptr::attach.
  scoped_refptr<BrowserProxy> result;
  result.swap(&browser);
  return result;
}

bool WindowProxy::IsMaximized(bool* maximized) {
  if (!is_valid())
    return false;

  bool result = false;

  sender_->Send(new AutomationMsg_IsWindowMaximized(handle_, maximized,
                                                    &result));
  return result;
}
