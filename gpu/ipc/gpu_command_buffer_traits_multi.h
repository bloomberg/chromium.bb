// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here.
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/gpu_export.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_STRUCT_TRAITS_BEGIN(gpu::Capabilities)
  IPC_STRUCT_TRAITS_MEMBER(post_sub_buffer)
  IPC_STRUCT_TRAITS_MEMBER(egl_image_external)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_bgra8888)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1)
  IPC_STRUCT_TRAITS_MEMBER(texture_format_etc1_npot)
  IPC_STRUCT_TRAITS_MEMBER(texture_rectangle)
  IPC_STRUCT_TRAITS_MEMBER(iosurface)
  IPC_STRUCT_TRAITS_MEMBER(texture_usage)
  IPC_STRUCT_TRAITS_MEMBER(texture_storage)
  IPC_STRUCT_TRAITS_MEMBER(discard_framebuffer)
  IPC_STRUCT_TRAITS_MEMBER(sync_query)
  IPC_STRUCT_TRAITS_MEMBER(image)
  IPC_STRUCT_TRAITS_MEMBER(blend_equation_advanced)
  IPC_STRUCT_TRAITS_MEMBER(blend_equation_advanced_coherent)
IPC_STRUCT_TRAITS_END()
