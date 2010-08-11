// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Short / basic implementation to simulate rendering H.264 frames outputs by
// MF's H.264 decoder to screen.

#ifndef MEDIA_MF_BASIC_RENDERER_H_
#define MEDIA_MF_BASIC_RENDERER_H_

#include <d3d9.h>

#include "base/scoped_ptr.h"
#include "base/scoped_comptr_win.h"
#include "media/base/video_frame.h"
#include "media/mf/mft_h264_decoder.h"

namespace media {

class MftRenderer : public base::RefCountedThreadSafe<MftRenderer> {
 public:
  explicit MftRenderer(MftH264Decoder* decoder) : decoder_(decoder) {}
  virtual ~MftRenderer() {}
  virtual void ProcessFrame(scoped_refptr<VideoFrame> frame) = 0;
  virtual void StartPlayback() = 0;
  virtual void StopPlayback() = 0;

 protected:
  scoped_refptr<MftH264Decoder> decoder_;
};

// This renderer does nothing with the frame except discarding it.
class NullRenderer : public MftRenderer {
 public:
  explicit NullRenderer(MftH264Decoder* decoder);
  virtual ~NullRenderer();
  virtual void ProcessFrame(scoped_refptr<VideoFrame> frame);
  virtual void StartPlayback();
  virtual void StopPlayback();
};

// This renderer does a basic playback by drawing to |window_|. It tries to
// respect timing specified in the recevied VideoFrames.
class BasicRenderer : public MftRenderer {
 public:
  explicit BasicRenderer(MftH264Decoder* decoder,
                         HWND window, IDirect3DDevice9* device);
  virtual ~BasicRenderer();
  virtual void ProcessFrame(scoped_refptr<VideoFrame> frame);
  virtual void StartPlayback();
  virtual void StopPlayback();

 private:
  HWND window_;
  ScopedComPtr<IDirect3DDevice9> device_;
};

}  // namespace media

#endif  // MEDIA_MF_BASIC_RENDERER_H_
