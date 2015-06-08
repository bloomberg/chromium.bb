// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_
#define CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"

// This class is used to report via UMA the Android kernel version and
// level of seccomp-bpf support. The operations are performed on the
// blocking thread pool.
class SeccompSupportDetector
    : public base::RefCountedThreadSafe<SeccompSupportDetector> {
 public:
  // Starts the detection process. This should be called once per browser
  // session. This is safe to call from any thread.
  static void StartDetection();

 private:
  friend class base::RefCountedThreadSafe<SeccompSupportDetector>;

  SeccompSupportDetector();
  ~SeccompSupportDetector();

  // Called on the blocking thread pool. This reads the utsname and records
  // the kernel version.
  void DetectKernelVersion();

  // Called on the blocking thread pool. This tests whether the system
  // supports PR_SET_SECCOMP.
  void DetectSeccomp();

  DISALLOW_COPY_AND_ASSIGN(SeccompSupportDetector);
};

#endif  // CHROME_BROWSER_ANDROID_SECCOMP_SUPPORT_DETECTOR_H_
