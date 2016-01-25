// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/zygote_host/zygote_host_impl_linux.h"

#include "base/allocator/allocator_extension.h"
#include "base/files/file_enumerator.h"
#include "base/process/kill.h"
#include "base/process/memory.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/content_browser_client.h"
#include "sandbox/linux/suid/common/sandbox.h"

namespace content {

// static
ZygoteHost* ZygoteHost::GetInstance() {
  return ZygoteHostImpl::GetInstance();
}

ZygoteHostImpl::ZygoteHostImpl()
    : use_suid_sandbox_for_adj_oom_score_(false),
      sandbox_binary_(),
      zygote_pids_lock_(),
      zygote_pids_() {}

ZygoteHostImpl::~ZygoteHostImpl() {}

// static
ZygoteHostImpl* ZygoteHostImpl::GetInstance() {
  return base::Singleton<ZygoteHostImpl>::get();
}

void ZygoteHostImpl::Init(const std::string& sandbox_cmd) {
  sandbox_binary_ = sandbox_cmd;
}

void ZygoteHostImpl::AddZygotePid(pid_t pid) {
  base::AutoLock lock(zygote_pids_lock_);
  zygote_pids_.insert(pid);
}

bool ZygoteHostImpl::IsZygotePid(pid_t pid) {
  base::AutoLock lock(zygote_pids_lock_);
  return zygote_pids_.find(pid) != zygote_pids_.end();
}

const std::string& ZygoteHostImpl::SandboxCommand() const {
  return sandbox_binary_;
}

void ZygoteHostImpl::SetRendererSandboxStatus(int status) {
  renderer_sandbox_status_ = status;
}

int ZygoteHostImpl::GetRendererSandboxStatus() const {
  return renderer_sandbox_status_;
}

#if !defined(OS_OPENBSD)
void ZygoteHostImpl::AdjustRendererOOMScore(base::ProcessHandle pid,
                                            int score) {
  // 1) You can't change the oom_score_adj of a non-dumpable process
  //    (EPERM) unless you're root. Because of this, we can't set the
  //    oom_adj from the browser process.
  //
  // 2) We can't set the oom_score_adj before entering the sandbox
  //    because the zygote is in the sandbox and the zygote is as
  //    critical as the browser process. Its oom_adj value shouldn't
  //    be changed.
  //
  // 3) A non-dumpable process can't even change its own oom_score_adj
  //    because it's root owned 0644. The sandboxed processes don't
  //    even have /proc, but one could imagine passing in a descriptor
  //    from outside.
  //
  // So, in the normal case, we use the SUID binary to change it for us.
  // However, Fedora (and other SELinux systems) don't like us touching other
  // process's oom_score_adj (or oom_adj) values
  // (https://bugzilla.redhat.com/show_bug.cgi?id=581256).
  //
  // The offical way to get the SELinux mode is selinux_getenforcemode, but I
  // don't want to add another library to the build as it's sure to cause
  // problems with other, non-SELinux distros.
  //
  // So we just check for files in /selinux. This isn't foolproof, but it's not
  // bad and it's easy.

  static bool selinux;
  static bool selinux_valid = false;

  if (!selinux_valid) {
    const base::FilePath kSelinuxPath("/selinux");
    base::FileEnumerator en(kSelinuxPath, false, base::FileEnumerator::FILES);
    bool has_selinux_files = !en.Next().empty();

    selinux = access(kSelinuxPath.value().c_str(), X_OK) == 0 &&
              has_selinux_files;
    selinux_valid = true;
  }

  if (use_suid_sandbox_for_adj_oom_score_ && !selinux) {
    // If heap profiling is running, these processes are not exiting, at least
    // on ChromeOS. The easiest thing to do is not launch them when profiling.
    // TODO(stevenjb): Investigate further and fix.
    if (base::allocator::IsHeapProfilerRunning())
      return;

    std::vector<std::string> adj_oom_score_cmdline;
    adj_oom_score_cmdline.push_back(sandbox_binary_);
    adj_oom_score_cmdline.push_back(sandbox::kAdjustOOMScoreSwitch);
    adj_oom_score_cmdline.push_back(base::Int64ToString(pid));
    adj_oom_score_cmdline.push_back(base::IntToString(score));

    base::Process sandbox_helper_process;
    base::LaunchOptions options;

    // sandbox_helper_process is a setuid binary.
    options.allow_new_privs = true;

    sandbox_helper_process =
        base::LaunchProcess(adj_oom_score_cmdline, options);
    if (sandbox_helper_process.IsValid())
      base::EnsureProcessGetsReaped(sandbox_helper_process.Pid());
  } else if (!use_suid_sandbox_for_adj_oom_score_) {
    if (!base::AdjustOOMScore(pid, score))
      PLOG(ERROR) << "Failed to adjust OOM score of renderer with pid " << pid;
  }
}
#endif

}  // namespace content
