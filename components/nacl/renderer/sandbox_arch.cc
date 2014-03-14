// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace nacl {

const char* GetSandboxArch() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return "arm";
#elif defined(ARCH_CPU_MIPS_FAMILY)
  return "mips32";
#elif defined(ARCH_CPU_X86_FAMILY)

#if defined(OS_WIN)
  // We have to check the host architecture on Windows.
  // See sandbox_isa.h for an explanation why.
  if (base::win::OSInfo::GetInstance()->architecture() ==
      base::win::OSInfo::X64_ARCHITECTURE)
    return "x86-64";
  else
    return "x86-32";
#elif ARCH_CPU_64_BITS
  return "x86-64";
#else
  return "x86-32";
#endif  // defined(OS_WIN)

#else
#error Architecture not supported.
#endif
}

}  // namespace nacl
