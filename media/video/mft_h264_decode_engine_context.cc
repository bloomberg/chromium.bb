// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/mft_h264_decode_engine_context.h"

#include <algorithm>
#include <vector>

#include <d3d9.h>

#include "base/task.h"
#include "media/base/callback.h"

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")

using base::TimeDelta;

namespace media {

static D3DFORMAT VideoFrameToD3DFormat(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::RGB555:
      return D3DFMT_X1R5G5B5;
    case VideoFrame::RGB565:
      return D3DFMT_R5G6B5;
    case VideoFrame::RGB32:
      return D3DFMT_X8R8G8B8;
    case VideoFrame::RGBA:
      return D3DFMT_A8R8G8B8;
    default:
      // Note that although there is a corresponding type for VideoFrame::RGB24
      // (D3DFMT_R8G8B8), it is not supported by render targets.
      NOTREACHED() << "Unsupported format";
      return D3DFMT_UNKNOWN;
  }
}

static IDirect3DTexture9* GetTexture(scoped_refptr<VideoFrame> frame) {
  return static_cast<IDirect3DTexture9*>(frame->d3d_texture(0));
}

static void ReleaseTexture(scoped_refptr<VideoFrame> frame) {
  GetTexture(frame)->Release();
}

static void ReleaseTextures(
    const std::vector<scoped_refptr<VideoFrame> >& frames) {
  std::for_each(frames.begin(), frames.end(), ReleaseTexture);
}

MftH264DecodeEngineContext::MftH264DecodeEngineContext(HWND device_window)
    : initialized_(false),
      device_window_(device_window),
      d3d9_(NULL),
      device_(NULL) {
  DCHECK(device_window);
}

MftH264DecodeEngineContext::~MftH264DecodeEngineContext() {
}

// TODO(imcheng): This should set the success variable once the API is
// finalized.
void MftH264DecodeEngineContext::Initialize(Task* task) {
  AutoTaskRunner runner(task);
  if (initialized_)
    return;
  d3d9_ = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d9_) {
    LOG(ERROR) << "Direct3DCreate9 failed";
    return;
  }

  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = 0;
  present_params.BackBufferHeight = 0;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = device_window_;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  HRESULT hr = d3d9_->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   device_window_,
                                   (D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                    D3DCREATE_MULTITHREADED),
                                   &present_params,
                                   device_.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "CreateDevice failed " << std::hex << hr;
    return;
  }
  initialized_ = true;
}

void* MftH264DecodeEngineContext::GetDevice() {
  return device_.get();
}

void MftH264DecodeEngineContext::AllocateVideoFrames(
    int n, size_t width, size_t height, VideoFrame::Format format,
    std::vector<scoped_refptr<VideoFrame> >* frames,
    Task* task) {
  DCHECK(initialized_);
  DCHECK_GT(n, 0);
  DCHECK(frames);

  AutoTaskRunner runner(task);
  D3DFORMAT d3d_format = VideoFrameToD3DFormat(format);
  std::vector<scoped_refptr<VideoFrame> > temp_frames;
  temp_frames.reserve(n);
  HRESULT hr;
  for (int i = 0; i < n; i++) {
    IDirect3DTexture9* texture = NULL;
    hr = device_->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
                                d3d_format, D3DPOOL_DEFAULT, &texture, NULL);
    if (FAILED(hr)) {
      LOG(ERROR) << "CreateTexture " << i << " failed " << std::hex << hr;
      ReleaseTextures(temp_frames);
      return;
    }
    VideoFrame::D3dTexture texture_array[VideoFrame::kMaxPlanes] =
        { texture, texture, texture };
    scoped_refptr<VideoFrame> texture_frame;
    VideoFrame::CreateFrameD3dTexture(format, width, height, texture_array,
                                      TimeDelta(), TimeDelta(), &texture_frame);
    if (!texture_frame.get()) {
      LOG(ERROR) << "CreateFrameD3dTexture " << i << " failed";
      texture->Release();
      ReleaseTextures(temp_frames);
      return;
    }
    temp_frames.push_back(texture_frame);
  }
  frames->assign(temp_frames.begin(), temp_frames.end());
  managed_frames_.insert(managed_frames_.end(),
                         temp_frames.begin(), temp_frames.end());
}

bool MftH264DecodeEngineContext::UploadToVideoFrame(
    void* source, scoped_refptr<VideoFrame> frame) {
  DCHECK(initialized_);
  DCHECK(source);
  DCHECK(frame.get());

  IDirect3DSurface9* surface = static_cast<IDirect3DSurface9*>(source);
  IDirect3DTexture9* texture = GetTexture(frame);
  ScopedComPtr<IDirect3DSurface9> top_surface;
  HRESULT hr;
  hr = texture->GetSurfaceLevel(0, top_surface.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "GetSurfaceLevel failed " << std::hex << hr;
    return false;
  }
  hr = device_->StretchRect(surface, NULL, top_surface.get(), NULL,
                            D3DTEXF_NONE);
  if (FAILED(hr)) {
    LOG(ERROR) << "StretchRect failed " << std::hex << hr;
    return false;
  }
  return true;
}

void MftH264DecodeEngineContext::ReleaseAllVideoFrames() {
  ReleaseTextures(managed_frames_);
  managed_frames_.clear();
}

void MftH264DecodeEngineContext::Destroy(Task* task) {
  AutoTaskRunner runner(task);
}

}  // namespace media
