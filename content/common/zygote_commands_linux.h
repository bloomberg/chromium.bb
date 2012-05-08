// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ZYGOTE_COMMANDS_LINUX_H_
#define CONTENT_COMMON_ZYGOTE_COMMANDS_LINUX_H_

namespace content {

// Contents of the initial message sent from the zygote to the browser when it
// is ready to go.
static const char kZygoteHelloMessage[] = "ZYGOTE_OK";

// These are the command codes used on the wire between the browser and the
// zygote.
enum {
  // Fork off a new renderer.
  kZygoteCommandFork = 0,

  // Reap a renderer child.
  kZygoteCommandReap = 1,

  // Check what happend to a child process.
  kZygoteCommandGetTerminationStatus = 2,

  // Read a bitmask of kSandboxLinux*
  kZygoteCommandGetSandboxStatus = 3
};

}  // namespace content

#endif  // CONTENT_COMMON_ZYGOTE_COMMANDS_LINUX_H_
