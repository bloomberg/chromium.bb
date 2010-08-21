// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_MESSAGES_H_
#define CHROME_COMMON_GPU_MESSAGES_H_
#pragma once

#include "base/basictypes.h"
#include "base/process.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_native_window_handle.h"
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

  GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params()
      : renderer_id(0),
        render_view_id(0),
        window(NULL),
        width(0),
        height(0),
        identifier(0) {
  }
};
#endif

namespace IPC {
#if defined(OS_MACOSX)
template <>
struct ParamTraits<GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params> {
  typedef GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.renderer_id);
    WriteParam(m, p.render_view_id);
    WriteParam(m, p.window);
    WriteParam(m, p.width);
    WriteParam(m, p.height);
    WriteParam(m, p.identifier);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return ReadParam(m, iter, &p->renderer_id) &&
        ReadParam(m, iter, &p->render_view_id) &&
        ReadParam(m, iter, &p->window) &&
        ReadParam(m, iter, &p->width) &&
        ReadParam(m, iter, &p->height) &&
        ReadParam(m, iter, &p->identifier);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.renderer_id, l);
    l->append(", ");
    LogParam(p.render_view_id, l);
    l->append(", ");
    LogParam(p.window, l);
    l->append(", ");
    LogParam(p.width, l);
    l->append(", ");
    LogParam(p.height, l);
    l->append(", ");
    LogParam(p.identifier, l);
    l->append(")");
  }
};
#endif

template <>
struct ParamTraits<GPUInfo> {
  typedef GPUInfo param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteUInt32(p.vendor_id());
    m->WriteUInt32(p.device_id());
    m->WriteWString(p.driver_version());
    m->WriteUInt32(p.pixel_shader_version());
    m->WriteUInt32(p.vertex_shader_version());
    m->WriteUInt32(p.gl_version());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    uint32 vendor_id;
    uint32 device_id;
    std::wstring driver_version;
    uint32 pixel_shader_version;
    uint32 vertex_shader_version;
    uint32 gl_version;
    bool ret = m->ReadUInt32(iter, &vendor_id);
    ret = ret && m->ReadUInt32(iter, &device_id);
    ret = ret && m->ReadWString(iter, &driver_version);
    ret = ret && m->ReadUInt32(iter, &pixel_shader_version);
    ret = ret && m->ReadUInt32(iter, &vertex_shader_version);
    ret = ret && m->ReadUInt32(iter, &gl_version);
    p->SetGraphicsInfo(vendor_id, device_id, driver_version,
                       pixel_shader_version, vertex_shader_version,
                       gl_version);
    return ret;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append(StringPrintf("<GPUInfo> %x %x %ls",
        p.vendor_id(), p.device_id(), p.driver_version().c_str()));
  }
};

template <>
struct ParamTraits<gpu::CommandBuffer::State> {
  typedef gpu::CommandBuffer::State param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.num_entries);
    m->WriteInt(p.get_offset);
    m->WriteInt(p.put_offset);
    m->WriteInt(p.token);
    m->WriteInt(p.error);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int32 temp;
    if (m->ReadInt(iter, &p->num_entries) &&
        m->ReadInt(iter, &p->get_offset) &&
        m->ReadInt(iter, &p->put_offset) &&
        m->ReadInt(iter, &p->token) &&
        m->ReadInt(iter, &temp)) {
      p->error = static_cast<gpu::error::Error>(temp);
      return true;
    } else {
      return false;
    }
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<CommandBuffer::State>");
  }
};
}  // namespace IPC

#define MESSAGES_INTERNAL_FILE \
    "chrome/common/gpu_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_GPU_MESSAGES_H_
