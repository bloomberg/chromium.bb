// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_TRANSFER_CACHE_ENTRY_ID_H_
#define GPU_COMMAND_BUFFER_COMMON_TRANSFER_CACHE_ENTRY_ID_H_

#include "gpu/command_buffer/common/discardable_handle.h"

namespace gpu {
using TransferCacheEntryId = ClientDiscardableHandle::Id;
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_TRANSFER_CACHE_ENTRY_ID_H_
