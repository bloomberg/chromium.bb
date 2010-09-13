// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_VIDEO_COMMON_H_
#define CHROME_COMMON_GPU_VIDEO_COMMON_H_

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "chrome/common/common_param_traits.h"

struct GpuVideoServiceInfoParam {
  // route id for GpuVideoService on GPU process side for this channel.
  int32 video_service_route_id;
  // route id for GpuVideoServiceHost on Render process side for this channel.
  int32 video_service_host_route_id;
  // TODO(jiesun): define capabilities of video service.
  int32 service_available;
};

struct GpuVideoDecoderInfoParam {
  // Context ID of the GLES2 context what this decoder should assicate with.
  int context_id;

  // Global decoder id.
  int32 decoder_id;

  // Route id for GpuVideoDecoder on GPU process side for this channel.
  int32 decoder_route_id;

  // TODO(hclam): Merge this ID with |decoder_route_id_|.
  // Route id for GpuVideoServiceHost on Render process side for this channel.
  int32 decoder_host_route_id;
};

struct GpuVideoDecoderInitParam {
  int32 codec_id;
  int32 width;
  int32 height;
  int32 profile;
  int32 level;
  int32 frame_rate_den;
  int32 frame_rate_num;
  int32 aspect_ratio_den;
  int32 aspect_ratio_num;
};

struct GpuVideoDecoderInitDoneParam {
  enum SurfaceType {
    SurfaceTypeSystemMemory,
    SurfaceTypeGlTexture,
    SurfaceTypeD3dTexture,
  };
  enum SurfaceFormat {
    SurfaceFormat_YV12,
    SurfaceFormat_NV12,
    SurfaceFormat_RGBA,
  };
  int32 success;  // other parameter is only meaningful when this is true.
  int32 provides_buffer;
  int32 format;
  int32 surface_type;
  int32 stride;
  int32 input_buffer_size;
  int32 output_buffer_size;
  base::SharedMemoryHandle input_buffer_handle;
  // we do not need this if hardware composition is ready.
  base::SharedMemoryHandle output_buffer_handle;
};

struct GpuVideoDecoderInputBufferParam {
  int64 timestamp;  // In unit of microseconds.
  int32 offset;
  int32 size;
  int32 flags;      // miscellaneous flag bit mask
};

struct GpuVideoDecoderOutputBufferParam {
  int64 timestamp;  // In unit of microseconds.
  int64 duration;   // In unit of microseconds.
  int32 flags;      // miscellaneous flag bit mask

  // TODO(hclam): This is really ugly and should be removed. Instead of sending
  // a texture id we should send a buffer id that signals that a buffer is ready
  // to be consumed. Before that we need API to establish the buffers.
  int32 texture;

  enum {
    kFlagsEndOfStream     = 0x00000001,
    kFlagsDiscontinuous   = 0x00000002,
  };
};

struct GpuVideoDecoderErrorInfoParam {
  int32 error_id;  // TODO(jiesun): define enum.
};

// TODO(jiesun): define this.
struct GpuVideoDecoderFormatChangeParam {
  int32 stride;
  int32 input_buffer_size;
  int32 output_buffer_size;
  base::SharedMemoryHandle input_buffer_handle;
  base::SharedMemoryHandle output_buffer_handle;
};

namespace IPC {

template <>
struct ParamTraits<GpuVideoServiceInfoParam> {
  typedef GpuVideoServiceInfoParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderInfoParam> {
  typedef GpuVideoDecoderInfoParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderInitParam> {
  typedef GpuVideoDecoderInitParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderInitDoneParam> {
  typedef GpuVideoDecoderInitDoneParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderInputBufferParam> {
  typedef GpuVideoDecoderInputBufferParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderOutputBufferParam> {
  typedef GpuVideoDecoderOutputBufferParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderErrorInfoParam> {
  typedef GpuVideoDecoderErrorInfoParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<GpuVideoDecoderFormatChangeParam> {
  typedef GpuVideoDecoderFormatChangeParam param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};
};

#endif  // CHROME_COMMON_GPU_VIDEO_COMMON_H_
