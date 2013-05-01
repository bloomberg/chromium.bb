// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/common/sandbox_linux.h"

template <typename T> struct DefaultSingletonTraits;
namespace sandbox { class SetuidSandboxClient; }

namespace content {

// A singleton class to represent and change our sandboxing state for the
// three main Linux sandboxes.
class LinuxSandbox {
 public:
  // This is a list of sandbox IPC methods which the renderer may send to the
  // sandbox host. See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC
  // This isn't the full list, values < 32 are reserved for methods called from
  // Skia.
  enum LinuxSandboxIPCMethods {
    METHOD_GET_FONT_FAMILY_FOR_CHARS = 32,
    METHOD_LOCALTIME = 33,
    METHOD_GET_CHILD_WITH_INODE = 34,
    METHOD_GET_STYLE_FOR_STRIKE = 35,
    METHOD_MAKE_SHARED_MEMORY_SEGMENT = 36,
    METHOD_MATCH_WITH_FALLBACK = 37,
  };

  // Get our singleton instance.
  static LinuxSandbox* GetInstance();

  // Do some initialization that can only be done before any of the sandboxes
  // are enabled. If using the setuid sandbox, this should be called manually
  // before the setuid sandbox is engaged.
  void PreinitializeSandbox();

  // Initialize the sandbox with the given pre-built configuration. Currently
  // seccomp-bpf and address space limitations (the setuid sandbox works
  // differently and is set-up in the Zygote). This will instantiate the
  // LinuxSandbox singleton if it doesn't already exist.
  static bool InitializeSandbox();

  // Returns the Status of the renderers' sandbox. Can only be queried after
  // going through PreinitializeSandbox(). This is a bitmask and uses the
  // constants defined in "enum LinuxSandboxStatus". Since the status needs to
  // be provided before the sandboxes are actually started, this returns what
  // will actually happen once the various Start* functions are called from
  // inside a renderer.
  int GetStatus() const;
  // Returns true if the current process is single-threaded or if the number
  // of threads cannot be determined.
  bool IsSingleThreaded() const;
  // Did we start Seccomp BPF?
  bool seccomp_bpf_started() const;

  // Simple accessor for our instance of the setuid sandbox. Will never return
  // NULL.
  // There is no StartSetuidSandbox(), the SetuidSandboxClient instance should
  // be used directly.
  sandbox::SetuidSandboxClient* setuid_sandbox_client() const;

  // Check the policy and eventually start the seccomp-bpf sandbox. This should
  // never be called with threads started. If we detect that threads have
  // started we will crash.
  bool StartSeccompBpf(const std::string& process_type);

  // Limit the address space of the current process (and its children).
  // to make some vulnerabilities harder to exploit.
  bool LimitAddressSpace(const std::string& process_type);

 private:
  friend struct DefaultSingletonTraits<LinuxSandbox>;

  // We must have been pre_initialized_ before using this.
  bool seccomp_bpf_supported() const;
  // The last part of the initialization is to make sure any temporary "hole"
  // in the sandbox is closed. For now, this consists of closing proc_fd_.
  void SealSandbox();

  // A file descriptor to /proc. It's dangerous to have it around as it could
  // allow for sandbox bypasses. It needs to be closed before we consider
  // ourselves sandboxed.
  int proc_fd_;
  bool seccomp_bpf_started_;
  // Did PreinitializeSandbox() run?
  bool pre_initialized_;
  bool seccomp_bpf_supported_;  // Accurate if pre_initialized_.
  scoped_ptr<sandbox::SetuidSandboxClient> setuid_sandbox_client_;

  ~LinuxSandbox();
  DISALLOW_IMPLICIT_CONSTRUCTORS(LinuxSandbox);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_H_

