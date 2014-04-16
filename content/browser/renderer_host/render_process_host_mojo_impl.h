// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_MOJO_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_MOJO_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "content/common/mojo/render_process.mojom.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"

namespace mojo {
namespace common {
class MojoChannelInit;
}
}

namespace content {

class RenderProcessHost;

// RenderProcessHostMojoImpl is responsible for initiating and maintaining the
// connection with the content side of RenderProcessHostMojo.
class RenderProcessHostMojoImpl : public RenderProcessHostMojo {
 public:
  explicit RenderProcessHostMojoImpl(RenderProcessHost* host);
  virtual ~RenderProcessHostMojoImpl();

  void SetWebUIHandle(int32 view_routing_id,
                      mojo::ScopedMessagePipeHandle handle);

  // Invoked when the RenderPorcessHost has established a channel.
  void OnProcessLaunched();

private:
  struct PendingHandle;

  // Establishes the mojo channel to the renderer.
  void CreateMojoChannel(base::ProcessHandle process_handle);

  RenderProcessHost* host_;

  // Used to establish the connection.
  scoped_ptr<mojo::common::MojoChannelInit> mojo_channel_init_;

  mojo::RemotePtr<content::RenderProcessMojo> render_process_mojo_;

  // If non-null we're waiting to send a WebUI handle to the renderer when
  // connected.
  scoped_ptr<PendingHandle> pending_handle_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostMojoImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_MOJO_IMPL_H_
