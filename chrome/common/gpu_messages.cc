// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_messages.h"

#include "chrome/common/gpu_info.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "ipc/ipc_channel_handle.h"

#define MESSAGES_INTERNAL_IMPL_FILE \
  "chrome/common/gpu_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"

namespace IPC {

void ParamTraits<GPUInfo>::Write(Message* m, const param_type& p) {
  m->WriteUInt32(p.vendor_id());
  m->WriteUInt32(p.device_id());
  m->WriteWString(p.driver_version());
  m->WriteUInt32(p.pixel_shader_version());
  m->WriteUInt32(p.vertex_shader_version());
  m->WriteUInt32(p.gl_version());
}

bool ParamTraits<GPUInfo>::Read(const Message* m, void** iter, param_type* p) {
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

void ParamTraits<GPUInfo>::Log(const param_type& p, std::string* l) {
  l->append(StringPrintf("<GPUInfo> %x %x %ls",
                         p.vendor_id(),
                         p.device_id(),
                         p.driver_version().c_str()));
}

} // namespace IPC
