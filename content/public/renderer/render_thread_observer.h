// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_THREAD_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_THREAD_OBSERVER_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {
class AssociatedInterfaceRegistry;

// Base class for objects that want to filter control IPC messages and get
// notified of events.
class CONTENT_EXPORT RenderThreadObserver {
 public:
  RenderThreadObserver() {}
  virtual ~RenderThreadObserver() {}

  // Allows handling incoming Mojo requests.
  virtual void RegisterMojoInterfaces(
      AssociatedInterfaceRegistry* associated_interfaces) {}
  virtual void UnregisterMojoInterfaces(
      AssociatedInterfaceRegistry* associated_interfaces) {}

  // Allows filtering of control messages.
  virtual bool OnControlMessageReceived(const IPC::Message& message);

  // Called when the renderer cache of the plugin list has changed.
  virtual void PluginListChanged() {}

  virtual void IdleNotification() {}

  // Called when the network state changes.
  virtual void NetworkStateChanged(bool online) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderThreadObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_THREAD_OBSERVER_H_
