// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/network_delegate.h"

#include "base/command_line.h"
#include "mojo/shell/switches.h"
#include "net/url_request/url_request.h"

namespace mojo {
namespace shell {

NetworkDelegate::NetworkDelegate() {
  DetachFromThread();
}

bool NetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                      const base::FilePath& path) const {
  // TODO(aa): We might want to add a --allow-file-urls or something, but
  // starting conservative.
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switches::kApp)
      == request.url().spec();
}

}  // namespace shell
}  // namespace mojo
