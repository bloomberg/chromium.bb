// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PROCESS_HOST_H_
#define CHROME_BROWSER_GPU_PROCESS_HOST_H_

#include <queue>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/child_process_launcher.h"
#include "chrome/common/gpu_native_window_handle.h"
#include "chrome/common/message_router.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"

class ChildProcessLauncher;
class CommandBufferProxy;

class GpuProcessHost : public IPC::Channel::Sender,
                       public IPC::Channel::Listener,
                       public ChildProcessLauncher::Client {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuProcessHost* Get();

  int32 GetNextRoutingId();

  // Creates the new remote view and returns the routing ID for the view, or 0
  // on failure.
  int32 NewRenderWidgetHostView(GpuNativeWindowHandle parent);

  // IPC::Channel::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched();

  // See documentation on MessageRouter for AddRoute and RemoveRoute
  void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32 routing_id);

  // Tells the GPU process to create a new channel for communication with a
  // renderer. Will asynchronously send message to object with given routing id
  // on completion.
  void EstablishGpuChannel(int renderer_id, int routing_id);

 private:
  friend struct DefaultSingletonTraits<GpuProcessHost>;

  // Used to queue pending channel requests.
  struct ChannelRequest {
    ChannelRequest(int renderer_id,
                   int routing_id) :
        renderer_id(renderer_id),
        routing_id(routing_id) {}
    // Used to identify the renderer. The ID is used instead of a pointer to
    // the RenderProcessHost in case it is destroyed while the request is
    // pending.
    // TODO(apatrick): investigate whether these IDs are used for future
    // render processes.
    int renderer_id;

    // Routing ID of object to receive reply message.
    int routing_id;
  };

  GpuProcessHost();
  virtual ~GpuProcessHost();

  void OnControlMessageReceived(const IPC::Message& message);

  // Message handlers.
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle);

  void ReplyToRenderer(int renderer_id,
                       int routing_id,
                       const IPC::ChannelHandle& channel);

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<ChannelRequest> sent_requests_;

  scoped_ptr<ChildProcessLauncher> child_process_;

  // A proxy for our IPC::Channel that lives on the IO thread (see
  // browser_process.h). This will be NULL if the class failed to connect.
  scoped_ptr<IPC::ChannelProxy> channel_;

  int last_routing_id_;

  MessageRouter router_;

  // Messages we queue while waiting for the process handle.  We queue them here
  // instead of in the channel so that we ensure they're sent after init related
  // messages that are sent once the process handle is available.  This is
  // because the queued messages may have dependencies on the init messages.
  std::queue<IPC::Message*> queued_messages_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

#endif  // CHROME_BROWSER_GPU_PROCESS_HOST_H_
