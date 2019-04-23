// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/macros.h"
#include "gpu/command_buffer/common/common_cmd_format.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {
namespace webgpu {
namespace cmds {

// Command buffer is GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT byte aligned.
#pragma pack(push, 4)
static_assert(GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT == 4,
              "pragma pack alignment must be equal to "
              "GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT");

#include "gpu/command_buffer/common/webgpu_cmd_format_autogen.h"

#pragma pack(pop)

}  // namespace cmds
}  // namespace webgpu
}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_H_
