// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_
#define CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_

#include <string>

#include "base/process/process_handle.h"
#include "services/shell/public/interfaces/shell.mojom.h"

namespace mojo {
class Connection;
}

namespace content {

class RenderProcessHost;

// Establish a mojo::Connection to the child process, using a pipe created for
// that purpose. Returns a token that should be passed to the child process and
// exchanged for a pipe there. That pipe can in turn be passed to
// MojoShellConnectionImpl::BindToMessagePipe() to initialize the child's
// shell connection.
std::string MojoConnectToChild(int child_process_id,
                               int instance_id,
                               RenderProcessHost* render_process_host);

// Returns a mojo connection to the provided render process host. This
// connection was opened when MojoConnectToChild() was called.
mojo::Connection* GetMojoConnection(RenderProcessHost* render_process_host);

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_CHILD_CONNECTION_H_
