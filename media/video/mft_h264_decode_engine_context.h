// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Video decode context for MftH264DecodeEngine. This context manages
// VideoFrame objects for the DXVA-enabled MFT H.264 decode engine, and
// converts its output (which is IDirect3DSurface9) into IDirect3DTexture9
// (wrapped in a VideoFrame object), which will be compatible with ANGLE.

#ifndef MEDIA_VIDEO_MFT_H264_DECODE_ENGINE_CONTEXT_H_
#define MEDIA_VIDEO_MFT_H264_DECODE_ENGINE_CONTEXT_H_

#include <vector>

#include "base/scoped_comptr_win.h"
#include "media/base/video_frame.h"
#include "media/video/video_decode_context.h"

class Task;

struct IDirect3D9;
extern "C" const GUID IID_IDirect3D9;
struct IDirect3DDevice9;
extern "C" const GUID IID_IDirect3DDevice9;

namespace media {

// TODO(imcheng): Make it implement VideoDecodeContext once the API
// is finalized.
class MftH264DecodeEngineContext {
 public:
  // Constructs a MftH264DecodeEngineContext with the D3D device attached
  // to |device_window|. This device does not own the window, so the caller
  // must destroy the window explicitly after the destruction of this object.
  explicit MftH264DecodeEngineContext(HWND device_window);
  virtual ~MftH264DecodeEngineContext();

  // TODO(imcheng): Is this a part of the API?
  virtual void Initialize(Task* task);

  // Gets the underlying IDirect3DDevice9.
  virtual void* GetDevice();

  // Allocates IDirect3DTexture9 objects wrapped in VideoFrame objects.
  virtual void AllocateVideoFrames(
      int n, size_t width, size_t height, VideoFrame::Format format,
      std::vector<scoped_refptr<VideoFrame> >* frames,
      Task* task);

  // TODO(imcheng): Make this follow the API once it is finalized.
  // Uploads the decoded frame (IDirect3DSurface9) to a VideoFrame allocated
  // by AllocateVideoFrames().
  virtual bool UploadToVideoFrame(void* source,
                                  scoped_refptr<VideoFrame> frame);
  virtual void ReleaseAllVideoFrames();
  virtual void Destroy(Task* task);

  bool initialized() const { return initialized_; }

 private:
  bool initialized_;
  HWND device_window_;
  std::vector<scoped_refptr<VideoFrame> > managed_frames_;
  ScopedComPtr<IDirect3D9, &IID_IDirect3D9> d3d9_;
  ScopedComPtr<IDirect3DDevice9, &IID_IDirect3DDevice9> device_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_MFT_H264_DECODE_ENGINE_CONTEXT_H_
