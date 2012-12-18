// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_REF_COUNTED_H_
#define GPU_COMMAND_BUFFER_CLIENT_REF_COUNTED_H_

// TODO(bbudge) The NaCl SRPC proxy can't depend on base, so we define our own
// ref_counted. This causes duplicate class warnings in the NaCl IPC proxy,
// which uses base. Remove the custom ref_counted class after NaCl has switched
// proxies.
#if defined(NACL_PPAPI_SRPC_PROXY)
#include "native_client/src/include/ref_counted.h"
namespace gpu {
using nacl::RefCountedThreadSafe;
}
#else
#include "base/memory/ref_counted.h"
namespace gpu {
using base::RefCountedThreadSafe;
}
#endif

#endif  // GPU_COMMAND_BUFFER_CLIENT_REF_COUNTED_H_
