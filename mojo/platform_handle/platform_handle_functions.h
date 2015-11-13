// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_FUNCTIONS_H_
#define MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_FUNCTIONS_H_

#include "mojo/platform_handle/platform_handle.h"
#include "mojo/platform_handle/platform_handle_exports.h"
#include "mojo/public/c/system/types.h"

extern "C" {

// Wraps |platform_handle| in a MojoHandle so that it can transported, returns
// true on success. This takes ownership of |platform_handle|, regardless of
// whether this succeeds.
PLATFORM_HANDLE_EXPORT MojoResult
MojoCreatePlatformHandleWrapper(MojoPlatformHandle platform_handle,
                                MojoHandle* wrapper);

PLATFORM_HANDLE_EXPORT MojoResult
MojoExtractPlatformHandle(MojoHandle wrapper,
                          MojoPlatformHandle* platform_handle);

}  // extern "C"

#endif  // MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_FUNCTIONS_H_
