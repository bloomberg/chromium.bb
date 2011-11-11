// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SECCOMP_SANDBOX_H_
#define CONTENT_COMMON_SECCOMP_SANDBOX_H_
#pragma once

// Seccomp enable/disable logic is centralized here.
// - We define SECCOMP_SANDBOX if seccomp is compiled in at all: currently,
//   on non-views (non-ChromeOS) non-ARM non-Clang Linux only.
// - If we have SECCOMP_SANDBOX, we provide SeccompSandboxEnabled() as
//   a run-time test to determine whether to turn on seccomp:
//   currently, on by default in debug builds and off by default in
//   release.

#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(ARCH_CPU_X86_FAMILY) && !defined(CHROMIUM_SELINUX) && \
  !defined(__clang__) && !defined(OS_CHROMEOS) && !defined(TOOLKIT_VIEWS) && \
  !defined(OS_OPENBSD)
#define SECCOMP_SANDBOX
#include "seccompsandbox/sandbox.h"
#endif

#if defined(SECCOMP_SANDBOX)
// Return true if seccomp is enabled.
static bool SeccompSandboxEnabled() {
  // TODO(evan): turn on for release too once we've flushed out all the bugs,
  // allowing us to delete this file entirely and just rely on the "disabled"
  // switch.
#ifdef NDEBUG
  // Off by default; allow turning on with a switch.
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSeccompSandbox);
#else
  // On by default; allow turning off with a switch.
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSeccompSandbox);
#endif  // NDEBUG
}
#endif  // SECCOMP_SANDBOX

#endif  // CONTENT_COMMON_SECCOMP_SANDBOX_H_
