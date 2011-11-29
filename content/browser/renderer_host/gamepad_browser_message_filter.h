// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_BROWSER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_BROWSER_MESSAGE_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/gamepad/gamepad_provider.h"

class GamepadBrowserMessageFilter : public BrowserMessageFilter {
 public:
  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  GamepadBrowserMessageFilter();

 private:
  virtual ~GamepadBrowserMessageFilter();

  void OnGamepadStartPolling(base::SharedMemoryHandle* renderer_handle);
  void OnGamepadStopPolling();

  scoped_refptr<gamepad::Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(GamepadBrowserMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_BROWSER_MESSAGE_FILTER_H_
