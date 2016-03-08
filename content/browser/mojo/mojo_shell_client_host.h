// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_

#include <string>

#include "base/process/process_handle.h"
#include "mojo/shell/public/cpp/identity.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"

namespace content {

class RenderProcessHost;

// Creates a communication channel between the external Mojo shell and the
// child. The server handle of this channel is shared with the external shell
// via Mojo IPC. |child_process_id| and |instance_id| are used to uniquify the
// child in the external shell's instance map.
//
// This returns a token that may be passed to the child process and exchanged
// for a pipe there. That pipe can in turn be passed to
// MojoShellConnectionImpl::BindToMessagePipe() to initialize the child's
// shell connection.
std::string RegisterChildWithExternalShell(
    int child_process_id,
    int instance_id,
    RenderProcessHost* render_process_host);

// Returns the Identity associated with an instance corresponding to the
// renderer process in shell. This Identity can be passed to Connect() to open a
// new connection to this renderer.
mojo::Identity GetMojoIdentity(RenderProcessHost* render_process_host);

// Constructs a Capability Filter for the renderer's application instance in the
// external shell. This contains the restrictions imposed on what applications
// and interfaces the renderer can see. The implementation lives in
// renderer_capability_filter.cc so that it can be subject to specific security
// review.
mojo::shell::mojom::CapabilityFilterPtr CreateCapabilityFilterForRenderer();

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_
