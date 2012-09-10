// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_BROWSER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_BROWSER_MESSAGE_FILTER_H_

#include "base/compiler_specific.h"
#include "base/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class GamepadService;
class RenderProcessHost;

class GamepadBrowserMessageFilter : public content::BrowserMessageFilter {
 public:
  GamepadBrowserMessageFilter();

  // content::BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~GamepadBrowserMessageFilter();

  void OnGamepadStartPolling(base::SharedMemoryHandle* renderer_handle);
  void OnGamepadStopPolling();

  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(GamepadBrowserMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_BROWSER_MESSAGE_FILTER_H_
