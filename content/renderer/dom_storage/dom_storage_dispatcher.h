// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_DOM_STORAGE_DISPATCHER_H_
#define CONTENT_RENDERER_DOM_STORAGE_DOM_STORAGE_DISPATCHER_H_
#pragma once

#include "ipc/ipc_channel.h"

struct DOMStorageMsg_Event_Params;

// Dispatches DomStorage related messages sent to a renderer process from the
// main browser process. There is one instance per child process. Messages
// are dispatched on the main renderer thread. The RenderThreadImpl
// creates an instance and delegates calls to it.
class DomStorageDispatcher : public IPC::Channel::Listener {
 public:
  DomStorageDispatcher() {}

  // IPC::Channel::Listener implementation
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // IPC message handlers
  void OnStorageEvent(const DOMStorageMsg_Event_Params& params);
};

#endif  // CONTENT_RENDERER_DOM_STORAGE_DOM_STORAGE_DISPATCHER_H_
