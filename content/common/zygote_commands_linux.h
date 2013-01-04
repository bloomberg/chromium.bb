// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ZYGOTE_COMMANDS_LINUX_H_
#define CONTENT_COMMON_ZYGOTE_COMMANDS_LINUX_H_

namespace content {

// Contents of the initial message sent from the zygote to the browser when it
// is ready to go.
static const char kZygoteHelloMessage[] = "ZYGOTE_OK";

// Maximum allowable length for messages sent to the zygote.
const size_t kZygoteMaxMessageLength = 8192;

// File descriptors initialized by the Zygote Host
const int kZygoteSocketPairFd = 3;
const int kZygoteRendererSocketFd = 5;
// This file descriptor is special. It is passed to the Zygote and a setuid
// helper will be called to locate the process of the Zygote on the system.
// This mechanism is used when multiple PID namespaces exist because of the
// setuid sandbox.
// It is very important that this file descriptor does not exist in multiple
// processes.
// This number must be kept in sync in sandbox/linux/suid/sandbox.c
const int kZygoteIdFd = 7;

// These are the command codes used on the wire between the browser and the
// zygote.
enum {
  // Fork off a new renderer.
  kZygoteCommandFork = 0,

  // Reap a renderer child.
  kZygoteCommandReap = 1,

  // Check what happened to a child process.
  kZygoteCommandGetTerminationStatus = 2,

  // Read a bitmask of kSandboxLinux*
  kZygoteCommandGetSandboxStatus = 3
};

}  // namespace content

#endif  // CONTENT_COMMON_ZYGOTE_COMMANDS_LINUX_H_
