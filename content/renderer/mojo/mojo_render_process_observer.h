// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_MOJO_RENDER_PROCESS_OBSERVER_H_
#define CONTENT_RENDERER_MOJO_MOJO_RENDER_PROCESS_OBSERVER_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/mojo/render_process.mojom.h"
#include "content/public/renderer/render_process_observer.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"

namespace mojo {
namespace common {
class MojoChannelInit;
}
namespace embedder {
struct ChannelInfo;
}
}

namespace content {

class RenderThread;

// RenderProcessObserver implementation that initializes the mojo channel when
// the right IPC is seen.
// MojoRenderProcessObserver deletes itself when the RenderProcess is shutdown.
class MojoRenderProcessObserver
    : public content::RenderProcessObserver,
      public RenderProcessMojo {
 public:
  MojoRenderProcessObserver(RenderThread* render_thread);

  // RenderProcessObserver overrides:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnRenderProcessShutdown() OVERRIDE;

 private:
  virtual ~MojoRenderProcessObserver();

  void OnChannelCreated(const IPC::PlatformFileForTransit& file);

  // RenderProcessMojo overrides:
  virtual void SetWebUIHandle(
      int32 view_routing_id,
      mojo::ScopedMessagePipeHandle web_ui_handle) OVERRIDE;

  content::RenderThread* render_thread_;

  scoped_ptr<mojo::common::MojoChannelInit> channel_init_;

  mojo::RemotePtr<content::RenderProcessHostMojo> render_process_host_mojo_;

  DISALLOW_COPY_AND_ASSIGN(MojoRenderProcessObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_MOJO_RENDER_PROCESS_OBSERVER_H_
