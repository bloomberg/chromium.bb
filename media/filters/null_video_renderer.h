// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_NULL_VIDEO_RENDERER_H_
#define MEDIA_FILTERS_NULL_VIDEO_RENDERER_H_

#include "media/filters/video_renderer_base.h"

namespace media {

class NullVideoRenderer : public VideoRendererBase {
 public:
  NullVideoRenderer();

  // VideoRendererBase implementation.
  virtual bool OnInitialize(VideoDecoder* decoder);
  virtual void OnStop(FilterCallback* callback);
  virtual void OnFrameAvailable();

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<NullVideoRenderer>;
  virtual ~NullVideoRenderer();

  DISALLOW_COPY_AND_ASSIGN(NullVideoRenderer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_NULL_VIDEO_RENDERER_H_
