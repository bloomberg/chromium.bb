// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_

#include <string>

#include "base/process/process_handle.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"

namespace content {

class RenderProcessHost;

// Creates a communication channel between the external Mojo shell and the
// child. The server handle of this channel is shared with the external shell
// via Mojo IPC. |child_process_id| is used to uniquify the child in the
// external shell's instance map.
void RegisterChildWithExternalShell(int child_process_id,
                                    RenderProcessHost* render_process_host);

// Returns the URL associated with an instance corresponding to the renderer
// process in the external shell. This URL can be passed to
// ConnectToApplication() to open a new connection to this renderer.
std::string GetMojoApplicationInstanceURL(
    RenderProcessHost* render_process_host);

// Shares a client handle to the Mojo Shell with the child via Chrome IPC.
void SendExternalMojoShellHandleToChild(base::ProcessHandle process_handle,
                                        RenderProcessHost* render_process_host);

// Constructs a Capability Filter for the renderer's application instance in the
// external shell. This contains the restrictions imposed on what applications
// and interfaces the renderer can see. The implementation lives in
// renderer_capability_filter.cc so that it can be subject to specific security
// review.
mojo::CapabilityFilterPtr CreateCapabilityFilterForRenderer();

// Used for the broker in the new EDK.
mojo::embedder::ScopedPlatformHandle RegisterProcessWithBroker(
    base::ProcessId pid);

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_
