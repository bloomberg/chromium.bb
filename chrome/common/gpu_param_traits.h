// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_PARAM_TRAITS_H_
#define CHROME_COMMON_GPU_PARAM_TRAITS_H_
#pragma once

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/dx_diag_node.h"
#include "chrome/common/gpu_create_command_buffer_config.h"
#include "chrome/common/gpu_info.h"
#include "gfx/native_widget_types.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "gpu/command_buffer/common/command_buffer.h"

#if defined(OS_MACOSX)
// Parameters for the GpuHostMsg_AcceleratedSurfaceSetIOSurface
// message, which has too many parameters to be sent with the
// predefined IPC macros.
struct GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params {
  int32 renderer_id;
  int32 render_view_id;
  gfx::PluginWindowHandle window;
  int32 width;
  int32 height;
  uint64 identifier;

  GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params();
};

struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params {
  int32 renderer_id;
  int32 render_view_id;
  gfx::PluginWindowHandle window;
  uint64 surface_id;
  int32 route_id;
  uint64 swap_buffers_count;

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params();
};
#endif

namespace IPC {
#if defined(OS_MACOSX)
template <>
struct ParamTraits<GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params> {
  typedef GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params> {
  typedef GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};
#endif

template <>
struct ParamTraits<GPUInfo> {
  typedef GPUInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GPUInfo::Level> {
  typedef GPUInfo::Level param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<DxDiagNode> {
  typedef DxDiagNode param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gpu::CommandBuffer::State> {
  typedef gpu::CommandBuffer::State param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GPUCreateCommandBufferConfig> {
  typedef GPUCreateCommandBufferConfig param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_GPU_PARAM_TRAITS_H_
