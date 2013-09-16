// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_CORE_H_
#define MOJO_PUBLIC_SYSTEM_CORE_H_

#include <stdint.h>

typedef uint32_t MojoHandle;

#ifdef __cplusplus
namespace mojo {

struct Handle { MojoHandle value; };

}  // namespace mojo
#endif

#endif  // MOJO_PUBLIC_SYSTEM_CORE_H_
