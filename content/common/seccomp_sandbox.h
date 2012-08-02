// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SECCOMP_SANDBOX_H_
#define CONTENT_COMMON_SECCOMP_SANDBOX_H_

// Seccomp enable/disable logic is centralized here.
// - We define SECCOMP_SANDBOX if seccomp is compiled in at all: currently,
//   on non-views (non-ChromeOS) non-ARM non-Clang Linux only.

#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY) && !defined(CHROMIUM_SELINUX) && \
  !defined(OS_CHROMEOS) && !defined(TOOLKIT_VIEWS) && !defined(OS_OPENBSD)
#define SECCOMP_SANDBOX
#include "sandbox/linux/seccomp-legacy/sandbox.h"
#endif

#endif  // CONTENT_COMMON_SECCOMP_SANDBOX_H_
