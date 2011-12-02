// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gamepad_browser_message_filter.h"

#include "content/browser/gamepad/gamepad_service.h"
#include "content/common/gamepad_messages.h"

namespace content {

GamepadBrowserMessageFilter::GamepadBrowserMessageFilter(
    content::RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host) {
}

GamepadBrowserMessageFilter::~GamepadBrowserMessageFilter() {
}

bool GamepadBrowserMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GamepadBrowserMessageFilter,
                           message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(GamepadHostMsg_StartPolling, OnGamepadStartPolling)
    IPC_MESSAGE_HANDLER(GamepadHostMsg_StopPolling, OnGamepadStopPolling)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void GamepadBrowserMessageFilter::OnGamepadStartPolling(
    base::SharedMemoryHandle* renderer_handle) {
  GamepadService* service = GamepadService::GetInstance();
  service->Start(NULL, render_process_host_);
  *renderer_handle = service->GetSharedMemoryHandle(peer_handle());
}

void GamepadBrowserMessageFilter::OnGamepadStopPolling() {
  // TODO(scottmg): Probably get rid of this message. We can't trust it will
  // arrive anyway if the renderer crashes, etc.
}

} // namespace content
