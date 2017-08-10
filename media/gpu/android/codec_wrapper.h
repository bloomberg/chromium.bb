// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_
#define MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/gpu/android/device_info.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/surface_texture_gl_owner.h"

namespace media {
class CodecWrapperImpl;

// A MediaCodec output buffer that can be released on any thread. Releasing a
// CodecOutputBuffer implicitly discards all CodecOutputBuffers that
// precede it in presentation order; i.e., the only supported use case is to
// render output buffers in order. This lets us return buffers to the codec as
// soon as we know we no longer need them.
class MEDIA_GPU_EXPORT CodecOutputBuffer {
 public:
  // Releases the buffer without rendering it.
  ~CodecOutputBuffer();

  // Releases this buffer and renders it to the surface.
  bool ReleaseToSurface();

  // The size of the image.
  gfx::Size size() const { return size_; }

 private:
  // Let CodecWrapperImpl call the constructor.
  friend class CodecWrapperImpl;
  CodecOutputBuffer(scoped_refptr<CodecWrapperImpl> codec,
                    int64_t id,
                    gfx::Size size);

  scoped_refptr<CodecWrapperImpl> codec_;
  int64_t id_;
  gfx::Size size_;
  DISALLOW_COPY_AND_ASSIGN(CodecOutputBuffer);
};

// This wraps a MediaCodecBridge and provides a pared down version of its
// interface. It also adds the following features:
// * It outputs CodecOutputBuffers from DequeueOutputBuffer() which can be
//   safely rendered on any thread, and that will release their buffers on
//   destruction. This lets us decode on one thread while rendering on another.
// * It maintains codec specific state like whether an error has occurred.
//
// CodecWrapper is not threadsafe, but the CodecOutputBuffers it outputs
// can be released on any thread.
class MEDIA_GPU_EXPORT CodecWrapper {
 public:
  // |codec| should be in the flushed state, i.e., freshly configured or after a
  // Flush(). |output_buffer_release_cb| will be run whenever an output buffer
  // is released back to the codec (whether it's rendered or not). This is a
  // signal that the codec might be ready to accept more input. It may be run on
  // any thread.
  CodecWrapper(std::unique_ptr<MediaCodecBridge> codec,
               base::Closure output_buffer_release_cb);
  ~CodecWrapper();

  // Takes the backing codec and discards all outstanding codec buffers. This
  // lets you tear down the codec while there are still CodecOutputBuffers
  // referencing |this|.
  std::unique_ptr<MediaCodecBridge> TakeCodec();

  // Whether the codec is in the flushed state.
  bool IsFlushed() const;

  // Whether an EOS has been queued but not yet dequeued.
  bool IsDraining() const;

  // Whether an EOS has been dequeued but the codec hasn't been flushed yet.
  bool IsDrained() const;

  // Whether there are any valid CodecOutputBuffers that have not been released.
  bool HasValidCodecOutputBuffers() const;

  // Releases currently dequeued codec buffers back to the codec without
  // rendering.
  void DiscardCodecOutputBuffers();

  // Whether the codec supports Flush().
  bool SupportsFlush(DeviceInfo* device_info) const;

  // See MediaCodecBridge documentation for the following.
  bool Flush();
  MediaCodecStatus QueueInputBuffer(int index,
                                    const uint8_t* data,
                                    size_t data_size,
                                    base::TimeDelta presentation_time);
  MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      const EncryptionScheme& encryption_scheme,
      base::TimeDelta presentation_time);
  void QueueEOS(int input_buffer_index);
  MediaCodecStatus DequeueInputBuffer(int* index);

  // Like MediaCodecBridge::DequeueOutputBuffer() but it outputs a
  // CodecOutputBuffer instead of an index. |*codec_buffer| must be null.
  // If this returns MEDIA_CODEC_OK then either |*end_of_stream| will be set to
  // true or |*codec_buffer| will be non-null. The EOS buffer is returned to the
  // codec immediately. Unlike MediaCodecBridge, this does not return
  // MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED or MEDIA_CODEC_OUTPUT_FORMAT_CHANGED. It
  // tries to dequeue another buffer instead.
  MediaCodecStatus DequeueOutputBuffer(
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      std::unique_ptr<CodecOutputBuffer>* codec_buffer);

  // Sets the given surface and returns MEDIA_CODEC_OK on success or
  // MEDIA_CODEC_ERROR on failure.
  MediaCodecStatus SetSurface(const base::android::JavaRef<jobject>& surface);

 private:
  scoped_refptr<CodecWrapperImpl> impl_;
  DISALLOW_COPY_AND_ASSIGN(CodecWrapper);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_WRAPPER_H_
