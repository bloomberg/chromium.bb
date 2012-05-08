// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ZYGOTE_HOST_IMPL_LINUX_H_
#define CONTENT_BROWSER_ZYGOTE_HOST_IMPL_LINUX_H_
#pragma once

#include <string>
#include <vector>

#include "base/global_descriptors_posix.h"
#include "base/process_util.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/zygote_host_linux.h"

template<typename Type>
struct DefaultSingletonTraits;

class CONTENT_EXPORT ZygoteHostImpl : public content::ZygoteHost {
 public:
  // Returns the singleton instance.
  static ZygoteHostImpl* GetInstance();

  void Init(const std::string& sandbox_cmd);

  // Tries to start a process of type indicated by process_type.
  // Returns its pid on success, otherwise
  // base::kNullProcessHandle;
  pid_t ForkRequest(const std::vector<std::string>& command_line,
                    const base::GlobalDescriptors::Mapping& mapping,
                    const std::string& process_type);
  void EnsureProcessTerminated(pid_t process);

  // Get the termination status (and, optionally, the exit code) of
  // the process. |exit_code| is set to the exit code of the child
  // process. (|exit_code| may be NULL.)
  base::TerminationStatus GetTerminationStatus(base::ProcessHandle handle,
                                               int* exit_code);

  // ZygoteHost implementation:
  virtual pid_t GetPid() const OVERRIDE;
  virtual pid_t GetSandboxHelperPid() const OVERRIDE;
  virtual int GetSandboxStatus() const OVERRIDE;
  virtual void AdjustRendererOOMScore(base::ProcessHandle process_handle,
                                      int score) OVERRIDE;
  virtual void AdjustLowMemoryMargin(int64 margin_mb) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ZygoteHostImpl>;
  ZygoteHostImpl();
  virtual ~ZygoteHostImpl();

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

#endif  // CONTENT_BROWSER_ZYGOTE_HOST_IMPL_LINUX_H_
