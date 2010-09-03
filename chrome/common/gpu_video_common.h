// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_VIDEO_COMMON_H_
#define CHROME_COMMON_GPU_VIDEO_COMMON_H_

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "chrome/common/common_param_traits.h"

class GpuVideoServiceInfoParam {
 public:
  // route id for GpuVideoService on GPU process side for this channel.
  int32 video_service_route_id_;
  // route id for GpuVideoServiceHost on Render process side for this channel.
  int32 video_service_host_route_id_;
  // TODO(jiesun): define capabilities of video service.
  int32 service_available_;
};

class GpuVideoDecoderInfoParam {
 public:
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

class GpuVideoDecoderInitParam {
 public:
  int32 codec_id_;
  int32 width_;
  int32 height_;
  int32 profile_;
  int32 level_;
  int32 frame_rate_den_;
  int32 frame_rate_num_;
  int32 aspect_ratio_den_;
  int32 aspect_ratio_num_;
};

class GpuVideoDecoderInitDoneParam {
 public:
  enum SurfaceType {
    SurfaceTypeSystemMemory,
    SurfaceTypeD3DSurface,
    SurfaceTypeEGLImage,
  };
  enum SurfaceFormat {
    SurfaceFormat_YV12,
    SurfaceFormat_NV12,
    SurfaceFormat_XRGB,
  };
  int32 success_;  // other parameter is only meaningful when this is true.
  int32 provides_buffer;
  int32 format_;
  int32 surface_type_;
  int32 stride_;
  int32 input_buffer_size_;
  int32 output_buffer_size_;
  base::SharedMemoryHandle input_buffer_handle_;
  // we do not need this if hardware composition is ready.
  base::SharedMemoryHandle output_buffer_handle_;
};

class GpuVideoDecoderInputBufferParam {
 public:
  int64 timestamp_;  // In unit of microseconds.
  int32 offset_;
  int32 size_;
  int32 flags_;      // miscellaneous flag bit mask
};

class GpuVideoDecoderOutputBufferParam {
 public:
  int64 timestamp_;  // In unit of microseconds.
  int64 duration_;   // In unit of microseconds.
  int32 flags_;      // miscellaneous flag bit mask

  enum {
    kFlagsEndOfStream     = 0x00000001,
    kFlagsDiscontinuous   = 0x00000002,
  };
};

class GpuVideoDecoderErrorInfoParam {
 public:
  int32 error_id;  // TODO(jiesun): define enum.
};

// TODO(jiesun): define this.
class GpuVideoDecoderFormatChangeParam {
 public:
  int32 stride_;
  int32 input_buffer_size_;
  int32 output_buffer_size_;
  base::SharedMemoryHandle input_buffer_handle_;
  base::SharedMemoryHandle output_buffer_handle_;
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
