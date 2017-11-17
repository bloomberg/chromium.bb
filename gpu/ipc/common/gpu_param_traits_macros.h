// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#define GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/flush_params.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "ipc/ipc_message_macros.h"
#include "url/ipc/url_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_STRUCT_TRAITS_BEGIN(gpu::FlushParams)
  IPC_STRUCT_TRAITS_MEMBER(route_id)
  IPC_STRUCT_TRAITS_MEMBER(put_offset)
  IPC_STRUCT_TRAITS_MEMBER(flush_id)
  IPC_STRUCT_TRAITS_MEMBER(snapshot_requested)
  IPC_STRUCT_TRAITS_MEMBER(sync_token_fences)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(gpu::SchedulingPriority,
                          gpu::SchedulingPriority::kLast)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::ContextResult,
                          gpu::ContextResult::kLastContextResult);

#endif  // GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
