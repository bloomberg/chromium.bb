// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GPU_VIDEO_COMMON_H_
#define CHROME_COMMON_GPU_VIDEO_COMMON_H_

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "chrome/common/common_param_traits.h"
#include "media/base/video_frame.h"

// This is used in messages when only buffer flag is meaningful.
extern const int32 kGpuVideoInvalidFrameId;

// Flags assigned to a video buffer for both input and output.
enum GpuVideoBufferFlag {
  kGpuVideoEndOfStream = 1 << 0,
  kGpuVideoDiscontinuous = 1 << 1,
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
  int32 success;  // other parameter is only meaningful when this is true.
  int32 input_buffer_size;
  base::SharedMemoryHandle input_buffer_handle;
};

struct GpuVideoDecoderInputBufferParam {
  int64 timestamp;  // In unit of microseconds.
  int32 offset;
  int32 size;
  int32 flags;  // Miscellaneous flag bit mask.
};

struct GpuVideoDecoderErrorInfoParam {
  int32 error_id;  // TODO(jiesun): define enum.
};

// TODO(jiesun): define this.
struct GpuVideoDecoderFormatChangeParam {
  int32 input_buffer_size;
  base::SharedMemoryHandle input_buffer_handle;
};

namespace IPC {

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

template <>
struct ParamTraits<media::VideoFrame::Format> {
  typedef media::VideoFrame::Format param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_GPU_VIDEO_COMMON_H_
