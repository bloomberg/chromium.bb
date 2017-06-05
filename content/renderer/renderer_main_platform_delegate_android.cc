// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <signal.h>

#include "base/android/build_info.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/seccomp_sandbox_status_android.h"
#include "sandbox/sandbox_features.h"

#if BUILDFLAG(USE_SECCOMP_BPF)
#include "content/common/sandbox_linux/android/sandbox_bpf_base_policy_android.h"
#include "content/renderer/seccomp_sandbox_status_android.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#endif

namespace content {

namespace {

// Scoper class to record a SeccompSandboxStatus UMA value.
class RecordSeccompStatus {
 public:
  RecordSeccompStatus() {
    SetSeccompSandboxStatus(SeccompSandboxStatus::NOT_SUPPORTED);
  }

  ~RecordSeccompStatus() {
    UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.RendererSandbox",
                              GetSeccompSandboxStatus(),
                              SeccompSandboxStatus::STATUS_MAX);
  }

  void set_status(SeccompSandboxStatus status) {
    SetSeccompSandboxStatus(status);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RecordSeccompStatus);
};

#if BUILDFLAG(USE_SECCOMP_BPF)
// Determines if the running device should support Seccomp, based on the Android
// SDK version.
bool IsSeccompBPFSupportedBySDK(const base::android::BuildInfo* info) {
  if (info->sdk_int() < 22) {
    // Seccomp was never available pre-Lollipop.
    return false;
  } else if (info->sdk_int() == 22) {
    // On Lollipop-MR1, only select Nexus devices have Seccomp available.
    const char* const kDevices[] = {
        "deb",   "flo",   "hammerhead", "mako",
        "manta", "shamu", "sprout",     "volantis",
    };

    for (auto* device : kDevices) {
      if (strcmp(device, info->device()) == 0) {
        return true;
      }
    }
  } else {
    // On Marshmallow and higher, Seccomp is required by CTS.
    return true;
  }
  return false;
}
#endif  // USE_SECCOMP_BPF

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  RecordSeccompStatus status_uma;

#if BUILDFLAG(USE_SECCOMP_BPF)
  auto* info = base::android::BuildInfo::GetInstance();

  // Determine if Seccomp is available via the Android SDK version.
  if (!IsSeccompBPFSupportedBySDK(info))
    return true;

  // Do run-time detection to ensure that support is present.
  if (!sandbox::SandboxBPF::SupportsSeccompSandbox(
          sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
    status_uma.set_status(SeccompSandboxStatus::DETECTION_FAILED);
    LOG(WARNING) << "Seccomp support should be present, but detection "
        << "failed. Continuing without Seccomp-BPF.";
    return true;
  }

  sig_t old_handler = signal(SIGSYS, SIG_DFL);
  if (old_handler != SIG_DFL) {
    // On Android O and later, the zygote applies a seccomp filter to all
    // apps. It has its own SIGSYS handler that must be un-hooked so that
    // the Chromium one can be used instead. If pre-O devices have a SIGSYS
    // handler, then warn about that.
    DLOG_IF(WARNING, info->sdk_int() < 26)
        << "Un-hooking existing SIGSYS handler before starting "
        << "Seccomp sandbox";
  }

  sandbox::SandboxBPF sandbox(new SandboxBPFBasePolicyAndroid());
  CHECK(
      sandbox.StartSandbox(sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED));

  status_uma.set_status(SeccompSandboxStatus::ENGAGED);
#endif
  return true;
}

}  // namespace content
