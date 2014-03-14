// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Routines for determining the most appropriate NaCl executable for
// the current CPU's architecture.

#ifndef COMPONENTS_NACL_RENDERER_SANDBOX_ARCH_H
#define COMPONENTS_NACL_RENDERER_SANDBOX_ARCH_H

namespace nacl {
// Returns the kind of SFI sandbox implemented by NaCl on this
// platform.  See the implementation in sandbox_arch.cc for possible
// values.
const char* GetSandboxArch();
}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_SANDBOX_ARCH_H
