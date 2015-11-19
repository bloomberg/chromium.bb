// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_

#include "base/process/process_handle.h"

namespace IPC {
class Sender;
}

namespace content {

// Creates a communication channel between the external Mojo shell and the
// child. The server handle of this channel is shared with the external shell
// via Mojo IPC and the client handle is shared with the child via Chrome IPC.
// |child_process_id| is used to uniquify the child in the external shell's
// instance map.
void RegisterChildWithExternalShell(int child_process_id,
                                    base::ProcessHandle process_handle,
                                    IPC::Sender* sender);

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CLIENT_HOST_H_
