// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_TRANSPORT_TEXTURE_SERVICE_H_
#define CONTENT_RENDERER_GPU_TRANSPORT_TEXTURE_SERVICE_H_

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "ipc/ipc_channel.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"

class RendererGLContext;
class TransportTextureHost;

// This class implements MessageFilter to dispatch IPC messages to
// TransportTextureHost.
class TransportTextureService : public IPC::ChannelProxy::MessageFilter,
                                public IPC::Message::Sender {
 public:
  TransportTextureService();
  virtual ~TransportTextureService();

  // IPC::ChannelProxy::MessageFilter implementations.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Called on ChildThread to create a TransportTextureHost.
  //
  // A routing ID for the GLES2 context needs to be provided. This is
  // important because the resources used needs to be shared with the GLES2
  // context corresponding to the RenderView.
  scoped_refptr<TransportTextureHost> CreateTransportTextureHost(
      RendererGLContext* context, int context_route_id);

  // Called by TransportTextureHost to remove a route.
  void RemoveRoute(int32 host_id);

  // IPC::Message::Sender Implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  typedef std::pair<int32, IPC::Channel::Listener*> PendingRoute;

  // Add a route from |host_id| to |listener|. If channel is not connected yet
  // then the information is saved.
  void AddRouteInternal(int32 host_id, IPC::Channel::Listener* listener);

  // Helper method to remove a route.
  void RemoveRouteInternal(int32 host_id);

  // Helper method to send an IPC message.
  void SendInternal(IPC::Message* msg);

  IPC::Channel* channel_;

  // Router to send messages to a TransportTextureHost. This is created when
  // the filter is added on the IO thread.
  scoped_ptr<MessageRouter> router_;

  // ID for the next TransportTextureHost.
  int32 next_host_id_;

  std::vector<PendingRoute> pending_routes_;
  std::vector<IPC::Message*> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(TransportTextureService);
};

#endif  // CONTENT_RENDERER_GPU_TRANSPORT_TEXTURE_SERVICE_H_
