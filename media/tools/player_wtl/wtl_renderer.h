// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_WTL_WTL_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_WTL_WTL_RENDERER_H_

#include "media/filters/video_renderer_base.h"

class WtlVideoWindow;

// Video renderer for media player.
class WtlVideoRenderer : public media::VideoRendererBase {
 public:
  explicit WtlVideoRenderer(WtlVideoWindow* window);

 protected:
  // VideoRendererBase implementation.
  virtual bool OnInitialize(media::VideoDecoder* decoder);
  virtual void OnStop(const base::Closure& callback);
  virtual void OnFrameAvailable();

 private:
  // Only allow to be deleted by reference counting.
  friend class scoped_refptr<WtlVideoRenderer>;
  virtual ~WtlVideoRenderer();

  WtlVideoWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WtlVideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_WTL_WTL_RENDERER_H_
