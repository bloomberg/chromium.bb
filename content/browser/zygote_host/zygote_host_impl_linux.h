// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_IMPL_LINUX_H_
#define CONTENT_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_IMPL_LINUX_H_

#include <sys/types.h>

#include <set>
#include <string>

#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/zygote_host_linux.h"

namespace base {
template<typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {

class CONTENT_EXPORT ZygoteHostImpl : public ZygoteHost {
 public:
  // Returns the singleton instance.
  static ZygoteHostImpl* GetInstance();

  void Init(const std::string& sandbox_cmd);

  // Retrieves the sandbox command passed into Init();
  const std::string& SandboxCommand() const;

  // Tells the ZygoteHost the PIDs of all the zygotes.
  void AddZygotePid(pid_t pid);

  // Returns whether or not this pid is the pid of a zygote.
  bool IsZygotePid(pid_t pid) override;

  bool ShouldUseNamespaceSandbox();

  void SetRendererSandboxStatus(int status);
  int GetRendererSandboxStatus() const override;
  void AdjustRendererOOMScore(base::ProcessHandle process_handle,
                              int score) override;

 private:
  friend struct base::DefaultSingletonTraits<ZygoteHostImpl>;

  ZygoteHostImpl();
  ~ZygoteHostImpl() override;

  int renderer_sandbox_status_;
  bool should_use_namespace_sandbox_;
  bool use_suid_sandbox_for_adj_oom_score_;
  std::string sandbox_binary_;
  // This lock protects the |zygote_pids_| set.
  base::Lock zygote_pids_lock_;
  // This is a set of PIDs representing all the running zygotes.
  std::set<pid_t> zygote_pids_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ZYGOTE_HOST_ZYGOTE_HOST_IMPL_LINUX_H_
