// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/seccomp_support_detector.h"

#include <stdio.h>
#include <sys/utsname.h>

#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;

enum AndroidSeccompStatus {
  DETECTION_FAILED,  // The process crashed during detection.
  NOT_SUPPORTED,     // Kernel has no seccomp support.
  SUPPORTED,         // Kernel has seccomp support.
  LAST_STATUS
};

// static
void SeccompSupportDetector::StartDetection() {
  // This is instantiated here, and then ownership is maintained by the
  // Closure objects when the object is being passed between threads. A
  // reference is also taken by the UtilityProcessHost, which will release
  // it when the process exits.
  scoped_refptr<SeccompSupportDetector> detector(new SeccompSupportDetector());
  BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&SeccompSupportDetector::DetectKernelVersion, detector));
}

SeccompSupportDetector::SeccompSupportDetector() : prctl_detected_(false) {
}

SeccompSupportDetector::~SeccompSupportDetector() {
}

void SeccompSupportDetector::DetectKernelVersion() {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  // This method will report the kernel major and minor versions by
  // taking the lower 16 bits of each version number and combining
  // the two into a 32-bit number.

  utsname uts;
  if (uname(&uts) == 0) {
    int major, minor;
    if (sscanf(uts.release, "%d.%d", &major, &minor) == 2) {
      int version = ((major & 0xFFFF) << 16) | (minor & 0xFFFF);
      UMA_HISTOGRAM_SPARSE_SLOWLY("Android.KernelVersion", version);
    }
  }

#if defined(USE_SECCOMP_BPF)
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SeccompSupportDetector::DetectSeccomp, this));
#else
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SeccompSupportDetector::OnDetectPrctl, this, false));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SeccompSupportDetector::OnDetectSyscall, this, false));
#endif
}

void SeccompSupportDetector::DetectSeccomp() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  content::UtilityProcessHost* utility_process_host =
      content::UtilityProcessHost::Create(
          this, base::MessageLoopProxy::current());
  utility_process_host->Send(new ChromeUtilityMsg_DetectSeccompSupport());
}

void SeccompSupportDetector::OnProcessCrashed(int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // The process crashed. Since prctl detection happens first, report which
  // probe failed.
  if (prctl_detected_) {
    UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.Syscall",
                              DETECTION_FAILED,
                              LAST_STATUS);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.Prctl",
                              DETECTION_FAILED,
                              LAST_STATUS);
  }
}

bool SeccompSupportDetector::OnMessageReceived(const IPC::Message& message) {
  bool handled = false;
  IPC_BEGIN_MESSAGE_MAP(SeccompSupportDetector, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_DetectSeccompSupport_ResultPrctl,
                        OnDetectPrctl)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_DetectSeccompSupport_ResultSyscall,
                        OnDetectSyscall)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SeccompSupportDetector::OnDetectPrctl(bool prctl_supported) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!prctl_detected_);

  prctl_detected_ = true;

  UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.Prctl",
                            prctl_supported ? SUPPORTED : NOT_SUPPORTED,
                            LAST_STATUS);
}

void SeccompSupportDetector::OnDetectSyscall(bool syscall_supported) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(prctl_detected_);

  UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.Syscall",
                            syscall_supported ? SUPPORTED : NOT_SUPPORTED,
                            LAST_STATUS);

  // The utility process will shutdown after this, and this object will
  // be deleted when the UtilityProcessHost releases its reference.
}
