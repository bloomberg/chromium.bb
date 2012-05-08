// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ZYGOTE_ZYGOTE_H_
#define CONTENT_ZYGOTE_ZYGOTE_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/process.h"

class Pickle;
class PickleIterator;

namespace content {

class ZygoteForkDelegate;

// This is the object which implements the zygote. The ZygoteMain function,
// which is called from ChromeMain, simply constructs one of these objects and
// runs it.
class Zygote {
 public:
  // The proc_fd_for_seccomp should be a file descriptor to /proc under the
  // seccomp sandbox. This is not needed when not using seccomp, and should be
  // -1 in those cases.
  Zygote(int sandbox_flags,
         ZygoteForkDelegate* helper,
         int proc_fd_for_seccomp);
  ~Zygote();

  bool ProcessRequests();

  static const int kBrowserDescriptor = 3;
  static const int kMagicSandboxIPCDescriptor = 5;

 private:
  // Returns true if the SUID sandbox is active.
  bool UsingSUIDSandbox() const;

  // ---------------------------------------------------------------------------
  // Requests from the browser...

  // Read and process a request from the browser. Returns true if we are in a
  // new process and thus need to unwind back into ChromeMain.
  bool HandleRequestFromBrowser(int fd);

  void HandleReapRequest(int fd, const Pickle& pickle, PickleIterator iter);

  void HandleGetTerminationStatus(int fd,
                                  const Pickle& pickle,
                                  PickleIterator iter);

  // This is equivalent to fork(), except that, when using the SUID sandbox, it
  // returns the real PID of the child process as it appears outside the
  // sandbox, rather than returning the PID inside the sandbox. Optionally, it
  // fills in uma_name et al with a report the helper wants to make via
  // UMA_HISTOGRAM_ENUMERATION.
  int ForkWithRealPid(const std::string& process_type,
                      std::vector<int>& fds,
                      const std::string& channel_switch,
                      std::string* uma_name,
                      int* uma_sample,
                      int* uma_boundary_value);

  // Unpacks process type and arguments from |pickle| and forks a new process.
  // Returns -1 on error, otherwise returns twice, returning 0 to the child
  // process and the child process ID to the parent process, like fork().
  base::ProcessId ReadArgsAndFork(const Pickle& pickle,
                                  PickleIterator iter,
                                  std::vector<int>& fds,
                                  std::string* uma_name,
                                  int* uma_sample,
                                  int* uma_boundary_value);

  // Handle a 'fork' request from the browser: this means that the browser
  // wishes to start a new renderer. Returns true if we are in a new process,
  // otherwise writes the child_pid back to the browser via |fd|. Writes a
  // child_pid of -1 on error.
  bool HandleForkRequest(int fd,
                         const Pickle& pickle,
                         PickleIterator iter,
                         std::vector<int>& fds);

  bool HandleGetSandboxStatus(int fd,
                              const Pickle& pickle,
                              PickleIterator iter);

  // In the SUID sandbox, we try to use a new PID namespace. Thus the PIDs
  // fork() returns are not the real PIDs, so we need to map the Real PIDS
  // into the sandbox PID namespace.
  typedef base::hash_map<base::ProcessHandle, base::ProcessHandle> ProcessMap;
  ProcessMap real_pids_to_sandbox_pids;

  const int sandbox_flags_;
  ZygoteForkDelegate* helper_;

  // File descriptor to proc under seccomp, -1 when not using seccomp.
  int proc_fd_for_seccomp_;

  // These might be set by helper_->InitialUMA. They supply a UMA enumeration
  // sample we should report on the first fork.
  std::string initial_uma_name_;
  int initial_uma_sample_;
  int initial_uma_boundary_value_;
};

}  // namespace content

#endif  // CONTENT_ZYGOTE_ZYGOTE_H_
