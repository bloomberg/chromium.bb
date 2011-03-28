// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_OBSERVER_H_
#define CONTENT_RENDERER_RENDER_PROCESS_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "ipc/ipc_message.h"

// Base class for objects that want to filter control IPC messages.
class RenderProcessObserver {
 public:
  virtual ~RenderProcessObserver();

  // Allows filtering of control messages.
  virtual bool OnControlMessageReceived(const IPC::Message& message);

  // Notification that the render process is shutting down.
  virtual void OnRenderProcessShutdown();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcessObserver);
};

#endif  // CONTENT_RENDERER_RENDER_PROCESS_OBSERVER_H_
