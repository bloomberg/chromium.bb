// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_video_common.h"

namespace IPC {

void ParamTraits<GpuVideoServiceInfoParam>::Write(
    Message* m, const GpuVideoServiceInfoParam& p) {
  m->WriteInt(p.video_service_route_id_);
  m->WriteInt(p.video_service_host_route_id_);
  m->WriteInt(p.service_available_);
}

bool ParamTraits<GpuVideoServiceInfoParam>::Read(
    const Message* m, void** iter, GpuVideoServiceInfoParam* r) {
  if (!m->ReadInt(iter, &r->video_service_route_id_) ||
      !m->ReadInt(iter, &r->video_service_host_route_id_) ||
      !m->ReadInt(iter, &r->service_available_))
    return false;
  return true;
}

void ParamTraits<GpuVideoServiceInfoParam>::Log(
    const GpuVideoServiceInfoParam& p, std::string* l) {
  l->append(StringPrintf("(%d, %d, %d)",
            p.video_service_route_id_,
            p.video_service_host_route_id_,
            p.service_available_));
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
  m->WriteInt(p.codec_id_);
  m->WriteInt(p.width_);
  m->WriteInt(p.height_);
}

bool ParamTraits<GpuVideoDecoderInitParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInitParam* r) {
  if (!m->ReadInt(iter, &r->codec_id_) ||
      !m->ReadInt(iter, &r->width_) ||
      !m->ReadInt(iter, &r->height_))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInitParam>::Log(
    const GpuVideoDecoderInitParam& p, std::string* l) {
  l->append(StringPrintf("(%d, %d %d)", p.codec_id_, p.width_, p.height_));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInitDoneParam>::Write(
    Message* m, const GpuVideoDecoderInitDoneParam& p) {
  m->WriteInt(p.success_);
  m->WriteInt(p.stride_);
  m->WriteInt(p.format_);
  m->WriteInt(p.surface_type_);
  m->WriteInt(p.input_buffer_size_);
  m->WriteInt(p.output_buffer_size_);
  IPC::ParamTraits<base::SharedMemoryHandle>::Write(
      m, p.input_buffer_handle_);
  IPC::ParamTraits<base::SharedMemoryHandle>::Write(
      m, p.output_buffer_handle_);
}

bool ParamTraits<GpuVideoDecoderInitDoneParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInitDoneParam* r) {
  if (!m->ReadInt(iter, &r->success_) ||
      !m->ReadInt(iter, &r->stride_) ||
      !m->ReadInt(iter, &r->format_) ||
      !m->ReadInt(iter, &r->surface_type_) ||
      !m->ReadInt(iter, &r->input_buffer_size_) ||
      !m->ReadInt(iter, &r->output_buffer_size_) ||
      !IPC::ParamTraits<base::SharedMemoryHandle>::Read(
          m, iter, &r->input_buffer_handle_) ||
      !IPC::ParamTraits<base::SharedMemoryHandle>::Read(
          m, iter, &r->output_buffer_handle_))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInitDoneParam>::Log(
    const GpuVideoDecoderInitDoneParam& p, std::string* l) {
  l->append(StringPrintf("(%d)", p.stride_));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInputBufferParam>::Write(
    Message* m, const GpuVideoDecoderInputBufferParam& p) {
  m->WriteInt64(p.timestamp_);
  m->WriteInt(p.offset_);
  m->WriteInt(p.size_);
}

bool ParamTraits<GpuVideoDecoderInputBufferParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInputBufferParam* r) {
  if (!m->ReadInt64(iter, &r->timestamp_) ||
      !m->ReadInt(iter, &r->offset_) ||
      !m->ReadInt(iter, &r->size_))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInputBufferParam>::Log(
    const GpuVideoDecoderInputBufferParam& p, std::string* l) {
  l->append(StringPrintf("(%d %d %d)",
                         static_cast<int>(p.timestamp_),
                         p.offset_, p.size_));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderOutputBufferParam>::Write(
    Message* m, const GpuVideoDecoderOutputBufferParam& p) {
  m->WriteInt64(p.timestamp_);
  m->WriteInt64(p.duration_);
  m->WriteInt(p.flags_);
}

bool ParamTraits<GpuVideoDecoderOutputBufferParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderOutputBufferParam* r) {
  if (!m->ReadInt64(iter, &r->timestamp_) ||
      !m->ReadInt64(iter, &r->duration_) ||
      !m->ReadInt(iter, &r->flags_))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderOutputBufferParam>::Log(
    const GpuVideoDecoderOutputBufferParam& p, std::string* l) {
  l->append(StringPrintf("(%d %d) %x",
                         static_cast<int>(p.timestamp_),
                         static_cast<int>(p.duration_),
                         p.flags_));
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
  m->WriteInt(p.input_buffer_size_);
  m->WriteInt(p.output_buffer_size_);
}

bool ParamTraits<GpuVideoDecoderFormatChangeParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderFormatChangeParam* r) {
  if (!m->ReadInt(iter, &r->input_buffer_size_) ||
      !m->ReadInt(iter, &r->output_buffer_size_))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderFormatChangeParam>::Log(
    const GpuVideoDecoderFormatChangeParam& p, std::string* l) {
  l->append(StringPrintf("(%d %d)", p.input_buffer_size_,
                         p.output_buffer_size_));
}
};
