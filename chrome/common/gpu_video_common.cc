// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_video_common.h"

namespace IPC {

void ParamTraits<GpuVideoServiceInfoParam>::Write(
    Message* m, const GpuVideoServiceInfoParam& p) {
  m->WriteInt(p.video_service_route_id);
  m->WriteInt(p.video_service_host_route_id);
  m->WriteInt(p.service_available);
}

bool ParamTraits<GpuVideoServiceInfoParam>::Read(
    const Message* m, void** iter, GpuVideoServiceInfoParam* r) {
  if (!m->ReadInt(iter, &r->video_service_route_id) ||
      !m->ReadInt(iter, &r->video_service_host_route_id) ||
      !m->ReadInt(iter, &r->service_available))
    return false;
  return true;
}

void ParamTraits<GpuVideoServiceInfoParam>::Log(
    const GpuVideoServiceInfoParam& p, std::string* l) {
  l->append(StringPrintf("(%d, %d, %d)",
            p.video_service_route_id,
            p.video_service_host_route_id,
            p.service_available));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInfoParam>::Write(
    Message* m, const GpuVideoDecoderInfoParam& p) {
  m->WriteInt(p.context_id);
  m->WriteInt(p.decoder_id);
  m->WriteInt(p.decoder_route_id);
  m->WriteInt(p.decoder_host_route_id);
}

bool ParamTraits<GpuVideoDecoderInfoParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInfoParam* r) {
  if (!m->ReadInt(iter, &r->context_id) ||
      !m->ReadInt(iter, &r->decoder_id) ||
      !m->ReadInt(iter, &r->decoder_route_id) ||
      !m->ReadInt(iter, &r->decoder_host_route_id))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInfoParam>::Log(
    const GpuVideoDecoderInfoParam& p, std::string* l) {
  l->append(StringPrintf("(%d, %d, %d)",
            p.decoder_id,
            p.decoder_route_id,
            p.decoder_host_route_id));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInitParam>::Write(
    Message* m, const GpuVideoDecoderInitParam& p) {
  m->WriteInt(p.codec_id);
  m->WriteInt(p.width);
  m->WriteInt(p.height);
}

bool ParamTraits<GpuVideoDecoderInitParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInitParam* r) {
  if (!m->ReadInt(iter, &r->codec_id) ||
      !m->ReadInt(iter, &r->width) ||
      !m->ReadInt(iter, &r->height))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInitParam>::Log(
    const GpuVideoDecoderInitParam& p, std::string* l) {
  l->append(StringPrintf("(%d, %d %d)", p.codec_id, p.width, p.height));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInitDoneParam>::Write(
    Message* m, const GpuVideoDecoderInitDoneParam& p) {
  m->WriteInt(p.success);
  m->WriteInt(p.stride);
  m->WriteInt(p.format);
  m->WriteInt(p.surface_type);
  m->WriteInt(p.input_buffer_size);
  m->WriteInt(p.output_buffer_size);
  IPC::ParamTraits<base::SharedMemoryHandle>::Write(
      m, p.input_buffer_handle);
  IPC::ParamTraits<base::SharedMemoryHandle>::Write(
      m, p.output_buffer_handle);
}

bool ParamTraits<GpuVideoDecoderInitDoneParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInitDoneParam* r) {
  if (!m->ReadInt(iter, &r->success) ||
      !m->ReadInt(iter, &r->stride) ||
      !m->ReadInt(iter, &r->format) ||
      !m->ReadInt(iter, &r->surface_type) ||
      !m->ReadInt(iter, &r->input_buffer_size) ||
      !m->ReadInt(iter, &r->output_buffer_size) ||
      !IPC::ParamTraits<base::SharedMemoryHandle>::Read(
          m, iter, &r->input_buffer_handle) ||
      !IPC::ParamTraits<base::SharedMemoryHandle>::Read(
          m, iter, &r->output_buffer_handle))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInitDoneParam>::Log(
    const GpuVideoDecoderInitDoneParam& p, std::string* l) {
  l->append(StringPrintf("(%d)", p.stride));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInputBufferParam>::Write(
    Message* m, const GpuVideoDecoderInputBufferParam& p) {
  m->WriteInt64(p.timestamp);
  m->WriteInt(p.offset);
  m->WriteInt(p.size);
}

bool ParamTraits<GpuVideoDecoderInputBufferParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInputBufferParam* r) {
  if (!m->ReadInt64(iter, &r->timestamp) ||
      !m->ReadInt(iter, &r->offset) ||
      !m->ReadInt(iter, &r->size))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInputBufferParam>::Log(
    const GpuVideoDecoderInputBufferParam& p, std::string* l) {
  l->append(StringPrintf("(%d %d %d)",
                         static_cast<int>(p.timestamp),
                         p.offset, p.size));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderOutputBufferParam>::Write(
    Message* m, const GpuVideoDecoderOutputBufferParam& p) {
  m->WriteInt64(p.timestamp);
  m->WriteInt64(p.duration);
  m->WriteInt(p.flags);
  m->WriteInt(p.texture);
}

bool ParamTraits<GpuVideoDecoderOutputBufferParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderOutputBufferParam* r) {
  if (!m->ReadInt64(iter, &r->timestamp) ||
      !m->ReadInt64(iter, &r->duration) ||
      !m->ReadInt(iter, &r->flags) ||
      !m->ReadInt(iter, &r->texture))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderOutputBufferParam>::Log(
    const GpuVideoDecoderOutputBufferParam& p, std::string* l) {
  l->append(StringPrintf("(%d %d) %x texture = x%d",
                         static_cast<int>(p.timestamp),
                         static_cast<int>(p.duration),
                         p.flags,
                         p.texture));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderErrorInfoParam>::Write(
    Message* m, const GpuVideoDecoderErrorInfoParam& p) {
  m->WriteInt(p.error_id);
}

bool ParamTraits<GpuVideoDecoderErrorInfoParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderErrorInfoParam* r) {
  if (!m->ReadInt(iter, &r->error_id))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderErrorInfoParam>::Log(
    const GpuVideoDecoderErrorInfoParam& p, std::string* l) {
  l->append(StringPrintf("(%d)", p.error_id));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderFormatChangeParam>::Write(
    Message* m, const GpuVideoDecoderFormatChangeParam& p) {
  m->WriteInt(p.input_buffer_size);
  m->WriteInt(p.output_buffer_size);
}

bool ParamTraits<GpuVideoDecoderFormatChangeParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderFormatChangeParam* r) {
  if (!m->ReadInt(iter, &r->input_buffer_size) ||
      !m->ReadInt(iter, &r->output_buffer_size))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderFormatChangeParam>::Log(
    const GpuVideoDecoderFormatChangeParam& p, std::string* l) {
  l->append(StringPrintf("(%d %d)", p.input_buffer_size,
                         p.output_buffer_size));
}
};
