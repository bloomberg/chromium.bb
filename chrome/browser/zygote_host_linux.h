// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ZYGOTE_HOST_LINUX_H_
#define CHROME_BROWSER_ZYGOTE_HOST_LINUX_H_
#pragma once

#include <unistd.h>

#include <string>
#include <vector>

#include "base/global_descriptors_posix.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/synchronization/lock.h"

template<typename Type>
struct DefaultSingletonTraits;

static const char kZygoteMagic[] = "ZYGOTE_OK";

// http://code.google.com/p/chromium/wiki/LinuxZygote

// The zygote host is the interface, in the browser process, to the zygote
// process.
class ZygoteHost {
 public:
  // Returns the singleton instance.
  static ZygoteHost* GetInstance();

  void Init(const std::string& sandbox_cmd);

  // Tries to start a renderer process.  Returns its pid on success, otherwise
  // base::kNullProcessHandle;
  pid_t ForkRenderer(const std::vector<std::string>& command_line,
                     const base::GlobalDescriptors::Mapping& mapping);
  void EnsureProcessTerminated(pid_t process);

  // Get the termination status (and, optionally, the exit code) of
  // the process. |exit_code| is set to the exit code of the child
  // process. (|exit_code| may be NULL.)
  base::TerminationStatus GetTerminationStatus(base::ProcessHandle handle,
                                               int* exit_code);

  // These are the command codes used on the wire between the browser and the
  // zygote.
  enum {
    kCmdFork = 0,                  // Fork off a new renderer.
    kCmdReap = 1,                  // Reap a renderer child.
    kCmdGetTerminationStatus = 2,  // Check what happend to a child process.
    kCmdGetSandboxStatus = 3,      // Read a bitmask of kSandbox*
  };

  // These form a bitmask which describes the conditions of the sandbox that
  // the zygote finds itself in.
  enum {
    kSandboxSUID = 1 << 0,     // SUID sandbox active
    kSandboxPIDNS = 1 << 1,    // SUID sandbox is using the PID namespace
    kSandboxNetNS = 1 << 2,    // SUID sandbox is using the network namespace
    kSandboxSeccomp = 1 << 3,  // seccomp sandbox active.
  };

  pid_t pid() const { return pid_; }

  // Returns an int which is a bitmask of kSandbox* values. Only valid after
  // the first render has been forked.
  int sandbox_status() const {
    if (have_read_sandbox_status_word_)
      return sandbox_status_;
    return 0;
  }

  // Adjust the OOM score of the given renderer's PID.
  void AdjustRendererOOMScore(base::ProcessHandle process_handle, int score);

 private:
  friend struct DefaultSingletonTraits<ZygoteHost>;
  ZygoteHost();
  ~ZygoteHost();

  ssize_t ReadReply(void* buf, size_t buflen);

  int control_fd_;  // the socket to the zygote
  // A lock protecting all communication with the zygote. This lock must be
  // acquired before sending a command and released after the result has been
  // received.
  base::Lock control_lock_;
  pid_t pid_;
  bool init_;
  bool using_suid_sandbox_;
  std::string sandbox_binary_;
  bool have_read_sandbox_status_word_;
  int sandbox_status_;
};

#endif  // CHROME_BROWSER_ZYGOTE_HOST_LINUX_H_
