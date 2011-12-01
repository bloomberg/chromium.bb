// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gamepad_browser_message_filter.h"

#include "content/common/gamepad_messages.h"

GamepadBrowserMessageFilter::GamepadBrowserMessageFilter() {
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
  if (!provider_) {
    provider_ = new content::GamepadProvider(NULL);
    provider_->Start();
  }
  *renderer_handle = provider_->GetRendererSharedMemoryHandle(peer_handle());
}

void GamepadBrowserMessageFilter::OnGamepadStopPolling() {
  // TODO(scottmg) Remove this message entirely?
  // Stop is currently handled by the refcount on provider_.
}
