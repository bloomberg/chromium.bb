// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_PRIVATE_THUNKS_H_
#define MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_PRIVATE_THUNKS_H_

#include <stddef.h>

#include "mojo/platform_handle/platform_handle_functions.h"

#pragma pack(push, 8)
struct MojoPlatformHandlePrivateThunks {
  size_t size;  // Should be set to sizeof(PlatformHandleThunks).
  MojoResult (*CreatePlatformHandleWrapper)(MojoPlatformHandle, MojoHandle*);
  MojoResult (*ExtractPlatformHandle)(MojoHandle, MojoPlatformHandle*);
};
#pragma pack(pop)

inline MojoPlatformHandlePrivateThunks MojoMakePlatformHandlePrivateThunks() {
  MojoPlatformHandlePrivateThunks system_thunks = {
      sizeof(MojoPlatformHandlePrivateThunks),
      MojoCreatePlatformHandleWrapper,
      MojoExtractPlatformHandle};
  return system_thunks;
}

typedef size_t (*MojoSetPlatformHandlePrivateThunksFn)(
    const struct MojoPlatformHandlePrivateThunks* thunks);

#endif  // MOJO_PLATFORM_HANDLE_PLATFORM_HANDLE_PRIVATE_THUNKS_H_
