// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_linux/bpf_cros_amd_gpu_policy_linux.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

using sandbox::bpf_dsl::Allow;
using sandbox::bpf_dsl::Arg;
using sandbox::bpf_dsl::Error;
using sandbox::bpf_dsl::If;
using sandbox::bpf_dsl::ResultExpr;
using sandbox::syscall_broker::BrokerFilePermission;

namespace content {

namespace {

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

void AddAmdGpuWhitelist(std::vector<BrokerFilePermission>* permissions) {
  static const char* kReadOnlyList[] = {"/etc/ld.so.cache",
                                        "/usr/lib64/libEGL.so.1",
                                        "/usr/lib64/libGLESv2.so.2"};
  int listSize = arraysize(kReadOnlyList);

  for (int i = 0; i < listSize; i++) {
    permissions->push_back(BrokerFilePermission::ReadOnly(kReadOnlyList[i]));
  }

  static const char* kReadWriteList[] = {
      "/dev/dri",
      "/dev/dri/card0",
      "/dev/dri/controlD64",
      "/dev/dri/renderD128",
      "/sys/class/drm/card0/device/config",
      "/sys/class/drm/controlD64/device/config",
      "/sys/class/drm/renderD128/device/config",
      "/usr/share/libdrm/amdgpu.ids"};

  listSize = arraysize(kReadWriteList);

  for (int i = 0; i < listSize; i++) {
    permissions->push_back(BrokerFilePermission::ReadWrite(kReadWriteList[i]));
  }

  static const char kCharDevices[] = "/sys/dev/char/";
  permissions->push_back(BrokerFilePermission::ReadOnlyRecursive(kCharDevices));
}

class CrosAmdGpuBrokerProcessPolicy : public CrosAmdGpuProcessPolicy {
 public:
  static sandbox::bpf_dsl::Policy* Create() {
    return new CrosAmdGpuBrokerProcessPolicy();
  }
  ~CrosAmdGpuBrokerProcessPolicy() override {}

  ResultExpr EvaluateSyscall(int system_call_number) const override;

 private:
  CrosAmdGpuBrokerProcessPolicy() {}
  DISALLOW_COPY_AND_ASSIGN(CrosAmdGpuBrokerProcessPolicy);
};

// A GPU broker policy is the same as a GPU policy with access, open,
// openat and in the non-Chrome OS case unlink allowed.
ResultExpr CrosAmdGpuBrokerProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_faccessat:
    case __NR_openat:
#if !defined(__aarch64__)
    case __NR_access:
    case __NR_open:
#if !defined(OS_CHROMEOS)
    // The broker process needs to able to unlink the temporary
    // files that it may create. This is used by DRI3.
    case __NR_unlink:
#endif  // !defined(OS_CHROMEOS)
#endif  // !define(__aarch64__)
      return Allow();
    default:
      return CrosAmdGpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

}  // namespace

CrosAmdGpuProcessPolicy::CrosAmdGpuProcessPolicy() {}

CrosAmdGpuProcessPolicy::~CrosAmdGpuProcessPolicy() {}

ResultExpr CrosAmdGpuProcessPolicy::EvaluateSyscall(int sysno) const {
  switch (sysno) {
    case __NR_fstatfs:
    case __NR_sched_setscheduler:
    case __NR_sysinfo:
    case __NR_uname:
#if !defined(__aarch64__)
    case __NR_getdents:
    case __NR_readlink:
    case __NR_stat:
#endif
      return Allow();
#if defined(__x86_64__)
    // Allow only AF_UNIX for |domain|.
    case __NR_socket:
    case __NR_socketpair: {
      const Arg<int> domain(0);
      return If(domain == AF_UNIX, Allow()).Else(Error(EPERM));
    }
#endif
    default:
      // Default to the generic GPU policy.
      return GpuProcessPolicy::EvaluateSyscall(sysno);
  }
}

bool CrosAmdGpuProcessPolicy::PreSandboxHook() {
  DCHECK(IsChromeOS());
  // Create a new broker process.
  DCHECK(!broker_process());

  // Add AMD-specific files to whitelist in the broker.
  std::vector<BrokerFilePermission> permissions;

  AddAmdGpuWhitelist(&permissions);
  InitGpuBrokerProcess(CrosAmdGpuBrokerProcessPolicy::Create, permissions);

  const int dlopen_flag = RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE;

  // Preload the amdgpu-dependent libraries.
  errno = 0;
  if (NULL == dlopen("libglapi.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(libglapi.so) failed with error: " << dlerror();
    return false;
  }
  if (NULL == dlopen("/usr/lib64/dri/swrast_dri.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(swrast_dri.so) failed with error: " << dlerror();
    return false;
  }
  if (NULL == dlopen("/usr/lib64/dri/radeonsi_dri.so", dlopen_flag)) {
    LOG(ERROR) << "dlopen(radeonsi_dri.so) failed with error: " << dlerror();
    return false;
  }

  return true;
}

}  // namespace content
