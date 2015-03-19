// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_
#define CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_

#include "base/compiler_specific.h"
#include "content/public/browser/utility_process_host_client.h"

// This class is used to report via UMA the Android kernel version and
// level of seccomp-bpf support. The kernel version is read from the blocking
// thread pool, while seccomp support is tested in a utility process, in case
// the probing causes a crash.
class SeccompSupportDetector : public content::UtilityProcessHostClient {
 public:
  // Starts the detection process. This should be called once per browser
  // session. This is safe to call from any thread.
  static void StartDetection();

 private:
  SeccompSupportDetector();
  ~SeccompSupportDetector() override;

  // Called on the blocking thread pool. This reads the utsname and records
  // the kernel version.
  void DetectKernelVersion();

  // Called on the IO thread. This starts a utility process to detect seccomp.
  void DetectSeccomp();

  // UtilityProcessHostClient:
  void OnProcessCrashed(int exit_code) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // OnDetectPrctl is always received before OnDetectSyscall.
  void OnDetectPrctl(bool prctl_supported);
  void OnDetectSyscall(bool syscall_supported);

  // Whether OnDetectPrctl was received.
  bool prctl_detected_;

  DISALLOW_COPY_AND_ASSIGN(SeccompSupportDetector);
};

#endif  // CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_
