// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_OBSERVER_H_
#define CONTENT_RENDERER_RENDER_PROCESS_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "ipc/ipc_message.h"
#include "content/common/content_export.h"

class GURL;

// Base class for objects that want to filter control IPC messages and get
// notified of events.
class CONTENT_EXPORT RenderProcessObserver : public IPC::Message::Sender {
 public:
  RenderProcessObserver();
  virtual ~RenderProcessObserver();

  // IPC::Message::Sender
  virtual bool Send(IPC::Message* message);

  // Allows filtering of control messages.
  virtual bool OnControlMessageReceived(const IPC::Message& message);

  // Notification that the render process is shutting down.
  virtual void OnRenderProcessShutdown();

  // Called right after the WebKit API is initialized.
  virtual void WebKitInitialized();

  virtual void IdleNotification();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderProcessObserver);
};

#endif  // CONTENT_RENDERER_RENDER_PROCESS_OBSERVER_H_
