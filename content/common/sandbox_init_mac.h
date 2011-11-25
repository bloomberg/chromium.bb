// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_INIT_MAC_H_
#define CONTENT_COMMON_SANDBOX_INIT_MAC_H_
#pragma once

namespace content {

// Initialize the sandbox for renderer, gpu, utility, worker, and plug-in
// processes, depending on the command line flags. Although The browser process
// is not sandboxed, this also needs to be called because it will initialize
// the broker code.
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing true is always
// returned.
bool InitializeSandbox();

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_INIT_MAC_H_
