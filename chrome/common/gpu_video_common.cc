// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_video_common.h"

const int32 kGpuVideoInvalidFrameId = -1;

namespace IPC {

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInitParam>::Write(
    Message* m, const GpuVideoDecoderInitParam& p) {
  WriteParam(m, p.codec_id);
  WriteParam(m, p.width);
  WriteParam(m, p.height);
}

bool ParamTraits<GpuVideoDecoderInitParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInitParam* r) {
  if (!ReadParam(m, iter, &r->codec_id) ||
      !ReadParam(m, iter, &r->width) ||
      !ReadParam(m, iter, &r->height))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInitParam>::Log(
    const GpuVideoDecoderInitParam& p, std::string* l) {
  l->append(base::StringPrintf("(%d, %d %d)", p.codec_id, p.width, p.height));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInitDoneParam>::Write(
    Message* m, const GpuVideoDecoderInitDoneParam& p) {
  WriteParam(m, p.success);
  WriteParam(m, p.input_buffer_size);
  WriteParam(m, p.input_buffer_handle);
}

bool ParamTraits<GpuVideoDecoderInitDoneParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInitDoneParam* r) {
  if (!ReadParam(m, iter, &r->success) ||
      !ReadParam(m, iter, &r->input_buffer_size) ||
      !ReadParam(m, iter, &r->input_buffer_handle))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInitDoneParam>::Log(
    const GpuVideoDecoderInitDoneParam& p, std::string* l) {
  l->append(base::StringPrintf("(%d %d)", p.success, p.input_buffer_size));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderInputBufferParam>::Write(
    Message* m, const GpuVideoDecoderInputBufferParam& p) {
  WriteParam(m, p.timestamp);
  WriteParam(m, p.offset);
  WriteParam(m, p.size);
}

bool ParamTraits<GpuVideoDecoderInputBufferParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderInputBufferParam* r) {
  if (!ReadParam(m, iter, &r->timestamp) ||
      !ReadParam(m, iter, &r->offset) ||
      !ReadParam(m, iter, &r->size))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderInputBufferParam>::Log(
    const GpuVideoDecoderInputBufferParam& p, std::string* l) {
  l->append(base::StringPrintf("(%d %d %d)",
                               static_cast<int>(p.timestamp),
                               p.offset, p.size));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderErrorInfoParam>::Write(
    Message* m, const GpuVideoDecoderErrorInfoParam& p) {
  WriteParam(m, p.error_id);
}

bool ParamTraits<GpuVideoDecoderErrorInfoParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderErrorInfoParam* r) {
  if (!ReadParam(m, iter, &r->error_id))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderErrorInfoParam>::Log(
    const GpuVideoDecoderErrorInfoParam& p, std::string* l) {
  l->append(base::StringPrintf("(%d)", p.error_id));
}

///////////////////////////////////////////////////////////////////////////////

void ParamTraits<GpuVideoDecoderFormatChangeParam>::Write(
    Message* m, const GpuVideoDecoderFormatChangeParam& p) {
  WriteParam(m, p.input_buffer_size);
}

bool ParamTraits<GpuVideoDecoderFormatChangeParam>::Read(
    const Message* m, void** iter, GpuVideoDecoderFormatChangeParam* r) {
  if (!ReadParam(m, iter, &r->input_buffer_size))
    return false;
  return true;
}

void ParamTraits<GpuVideoDecoderFormatChangeParam>::Log(
    const GpuVideoDecoderFormatChangeParam& p, std::string* l) {
  l->append(base::StringPrintf("%d", p.input_buffer_size));
}

///////////////////////////////////////////////////////////////////////////////
// Traits for media::VideoFrame::Format
void ParamTraits<media::VideoFrame::Format>::Write(
    Message* m, const param_type& p) {
  m->WriteInt(p);
}

bool ParamTraits<media::VideoFrame::Format>::Read(
    const Message* m, void** iter, param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<param_type>(type);
  return true;
}

void ParamTraits<media::VideoFrame::Format>::Log(
    const param_type& p, std::string* l) {
  std::string s;
  switch (p) {
    case media::VideoFrame::RGB555:
      s = "RGB555";
      break;
    case media::VideoFrame::RGB565:
      s = "RGB565";
      break;
    case media::VideoFrame::RGB24:
      s = "RGB24";
      break;
    case media::VideoFrame::RGB32:
      s = "RGB32";
      break;
    case media::VideoFrame::RGBA:
      s = "RGBA";
      break;
    case media::VideoFrame::YV12:
      s = "YV12";
      break;
    case media::VideoFrame::YV16:
      s = "YV16";
      break;
    case media::VideoFrame::NV12:
      s = "NV12";
      break;
    case media::VideoFrame::EMPTY:
      s = "EMPTY";
      break;
    case media::VideoFrame::ASCII:
      s = "ASCII";
      break;
    case media::VideoFrame::INVALID:
      s = "INVALID";
      break;
    default:
      NOTIMPLEMENTED();
      s = "UNKNOWN";
      break;
  }
  LogParam(s, l);
}

}  // namespace IPC
