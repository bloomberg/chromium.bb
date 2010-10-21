// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_messages.h"

#include "chrome/common/gpu_create_command_buffer_config.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/dx_diag_node.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "ipc/ipc_channel_handle.h"

#define MESSAGES_INTERNAL_IMPL_FILE \
  "chrome/common/gpu_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"

#if defined(OS_MACOSX)

// Parameters for the GpuHostMsg_AcceleratedSurfaceSetIOSurface
// message, which has too many parameters to be sent with the
// predefined IPC macros.
GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params::
    GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params()
        : renderer_id(0),
          render_view_id(0),
          window(NULL),
          width(0),
          height(0),
          identifier(0) {
}

#endif

namespace IPC {

#if defined(OS_MACOSX)

void ParamTraits<GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params> ::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.renderer_id);
  WriteParam(m, p.render_view_id);
  WriteParam(m, p.window);
  WriteParam(m, p.width);
  WriteParam(m, p.height);
  WriteParam(m, p.identifier);
}

bool ParamTraits<GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params> ::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return ReadParam(m, iter, &p->renderer_id) &&
      ReadParam(m, iter, &p->render_view_id) &&
      ReadParam(m, iter, &p->window) &&
      ReadParam(m, iter, &p->width) &&
      ReadParam(m, iter, &p->height) &&
      ReadParam(m, iter, &p->identifier);
}

void ParamTraits<GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params> ::Log(
     const param_type& p,
     std::string* l) {
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

#endif  // if defined(OS_MACOSX)

void ParamTraits<GPUInfo> ::Write(Message* m, const param_type& p) {
  m->WriteUInt32(p.vendor_id());
  m->WriteUInt32(p.device_id());
  m->WriteWString(p.driver_version());
  m->WriteUInt32(p.pixel_shader_version());
  m->WriteUInt32(p.vertex_shader_version());
  m->WriteUInt32(p.gl_version());
  m->WriteBool(p.can_lose_context());

#if defined(OS_WIN)
  ParamTraits<DxDiagNode> ::Write(m, p.dx_diagnostics());
#endif
}

bool ParamTraits<GPUInfo> ::Read(const Message* m, void** iter, param_type* p) {
  uint32 vendor_id;
  uint32 device_id;
  std::wstring driver_version;
  uint32 pixel_shader_version;
  uint32 vertex_shader_version;
  uint32 gl_version;
  bool can_lose_context;
  bool ret = m->ReadUInt32(iter, &vendor_id);
  ret = ret && m->ReadUInt32(iter, &device_id);
  ret = ret && m->ReadWString(iter, &driver_version);
  ret = ret && m->ReadUInt32(iter, &pixel_shader_version);
  ret = ret && m->ReadUInt32(iter, &vertex_shader_version);
  ret = ret && m->ReadUInt32(iter, &gl_version);
  ret = ret && m->ReadBool(iter, &can_lose_context);
  p->SetGraphicsInfo(vendor_id,
                     device_id,
                     driver_version,
                     pixel_shader_version,
                     vertex_shader_version,
                     gl_version,
                     can_lose_context);

#if defined(OS_WIN)
  DxDiagNode dx_diagnostics;
  ret = ret && ParamTraits<DxDiagNode> ::Read(m, iter, &dx_diagnostics);
  p->SetDxDiagnostics(dx_diagnostics);
#endif

  return ret;
}

void ParamTraits<GPUInfo> ::Log(const param_type& p, std::string* l) {
  l->append(StringPrintf("<GPUInfo> %x %x %ls %d",
                         p.vendor_id(),
                         p.device_id(),
                         p.driver_version().c_str(),
                         p.can_lose_context()));
}

void ParamTraits<DxDiagNode> ::Write(Message* m, const param_type& p) {
  ParamTraits<std::map<std::string, std::string> >::Write(m, p.values);
  ParamTraits<std::map<std::string, DxDiagNode> >::Write(m, p.children);
}

bool ParamTraits<DxDiagNode> ::Read(const Message* m,
                                    void** iter,
                                    param_type* p) {
  bool ret = ParamTraits<std::map<std::string, std::string> >::Read(
      m,
      iter,
      &p->values);
  ret = ret && ParamTraits<std::map<std::string, DxDiagNode> >::Read(
      m,
      iter,
      &p->children);
  return ret;
}

void ParamTraits<DxDiagNode> ::Log(const param_type& p, std::string* l) {
  l->append("<DxDiagNode>");
}

void ParamTraits<gpu::CommandBuffer::State> ::Write(Message* m,
                                                    const param_type& p) {
  m->WriteInt(p.num_entries);
  m->WriteInt(p.get_offset);
  m->WriteInt(p.put_offset);
  m->WriteInt(p.token);
  m->WriteInt(p.error);
}

bool ParamTraits<gpu::CommandBuffer::State> ::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
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

void ParamTraits<gpu::CommandBuffer::State> ::Log(const param_type& p,
                                                  std::string* l) {
  l->append("<CommandBuffer::State>");
}

void ParamTraits<GPUCreateCommandBufferConfig> ::Write(
    Message* m, const param_type& p) {
  m->WriteString(p.allowed_extensions);
  ParamTraits<std::vector<int> > ::Write(m, p.attribs);
}

bool ParamTraits<GPUCreateCommandBufferConfig> ::Read(
    const Message* m, void** iter, param_type* p) {
  if (!m->ReadString(iter, &p->allowed_extensions) ||
      !ParamTraits<std::vector<int> > ::Read(m, iter, &p->attribs)) {
    return false;
  }
  return true;
}

void ParamTraits<GPUCreateCommandBufferConfig> ::Log(
    const param_type& p, std::string* l) {
  l->append("<GPUCreateCommandBufferConfig>");
}

}  // namespace IPC
