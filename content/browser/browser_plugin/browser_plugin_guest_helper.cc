// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest_helper.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/common/drag_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"

namespace content {

BrowserPluginGuestHelper::BrowserPluginGuestHelper(
    BrowserPluginGuest* guest,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      guest_(guest) {
}

BrowserPluginGuestHelper::~BrowserPluginGuestHelper() {
}

bool BrowserPluginGuestHelper::OnMessageReceived(
    const IPC::Message& message) {
  if (ShouldForwardToBrowserPluginGuest(message))
    return guest_->OnMessageReceived(message);
  return false;
}

// static
bool BrowserPluginGuestHelper::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  switch (message.type()) {
    case DragHostMsg_UpdateDragCursor::ID:
    case ViewHostMsg_HasTouchEventHandlers::ID:
    case ViewHostMsg_SetCursor::ID:
 #if defined(OS_MACOSX)
    case ViewHostMsg_ShowPopup::ID:
 #endif
    case ViewHostMsg_ShowWidget::ID:
    case ViewHostMsg_TakeFocus::ID:
    case ViewHostMsg_UpdateFrameName::ID:
    case ViewHostMsg_UpdateRect::ID:
    case ViewHostMsg_LockMouse::ID:
    case ViewHostMsg_UnlockMouse::ID:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace content
