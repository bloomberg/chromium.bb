// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_
#pragma once

// This class lives on the UI thread and supports classes like the
// BackingStoreProxy, which must live on the UI thread. The IO thread
// portion of this class, the GpuProcessHost, is responsible for
// shuttling messages between the browser and GPU processes.

#include "base/singleton.h"
#include "chrome/common/gpu_native_window_handle.h"
#include "chrome/common/message_router.h"
#include "ipc/ipc_channel.h"
#include "gfx/native_widget_types.h"

class GpuProcessHostUIShim : public IPC::Channel::Sender,
                             public IPC::Channel::Listener {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHostUIShim* Get();

  int32 GetNextRoutingId();

  // Creates the new remote view and returns the routing ID for the view, or 0
  // on failure.
  int32 NewRenderWidgetHostView(GpuNativeWindowHandle parent);

  // IPC::Channel::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  // The GpuProcessHost causes this to be called on the UI thread to
  // dispatch the incoming messages from the GPU process, which are
  // actually received on the IO thread.
  virtual void OnMessageReceived(const IPC::Message& message);

  // See documentation on MessageRouter for AddRoute and RemoveRoute
  void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32 routing_id);

  // Sends a message to the browser process to collect the information from the
  // graphics card.
  void CollectGraphicsInfoAsynchronously();

 private:
  friend struct DefaultSingletonTraits<GpuProcessHostUIShim>;

  GpuProcessHostUIShim();
  virtual ~GpuProcessHostUIShim();

  int last_routing_id_;

  MessageRouter router_;
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_UI_SHIM_H_

