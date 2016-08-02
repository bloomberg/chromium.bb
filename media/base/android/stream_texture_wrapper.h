// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_STREAM_TEXTURE_WRAPPER_H_
#define MEDIA_BASE_ANDROID_STREAM_TEXTURE_WRAPPER_H_

#include "media/base/video_frame.h"

namespace media {
class VideoFrame;

// StreamTextureWrapper encapsulates a StreamTexture's creation, initialization
// and registration for later retrieval (in the Browser process).
//
// TODO(tguilbert): Support registering the underlying SurfaceTexture so it can
// be used by a MediaPlayer in the browser process. See crbug.com/627658.
class MEDIA_EXPORT StreamTextureWrapper {
 public:
  StreamTextureWrapper() {}

  // Initialize the underlying StreamTexture.
  // See StreamTextureWrapperImpl.
  virtual void Initialize(
      const base::Closure& received_frame_cb,
      const gfx::Size& natural_size,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      const base::Closure& init_cb) = 0;

  // Called whenever the video's natural size changes.
  // See StreamTextureWrapperImpl.
  virtual void UpdateTextureSize(const gfx::Size& natural_size) = 0;

  // Returns the latest frame.
  // See StreamTextureWrapperImpl.
  virtual scoped_refptr<VideoFrame> GetCurrentFrame() = 0;

  struct Deleter {
    inline void operator()(StreamTextureWrapper* ptr) const { ptr->Destroy(); }
  };

 protected:
  virtual ~StreamTextureWrapper() {}

  // Safely destroys the StreamTextureWrapper.
  // See StreamTextureWrapperImpl.
  virtual void Destroy() = 0;
};

typedef std::unique_ptr<StreamTextureWrapper, StreamTextureWrapper::Deleter>
    ScopedStreamTextureWrapper;

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_STREAM_TEXTURE_WRAPPER_H_
