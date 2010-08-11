// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mf/basic_renderer.h"

#include <d3d9.h>
#include <mfapi.h>
#include <mfidl.h>

#include "base/message_loop.h"
#include "base/scoped_comptr_win.h"
#include "base/time.h"
#include "media/base/yuv_convert.h"

// For MFGetService and MF_BUFFER_SERVICE (getting D3D surface from buffer)
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "strmiids.lib")

namespace media {

// Converts the given raw data buffer into RGB32 format, and drawing the result
// into the given window. This is only used when DXVA2 is not enabled.
// Returns: true on success.
bool ConvertToRGBAndDrawToWindow(HWND video_window, uint8* data, int width,
                                 int height, int stride) {
  CHECK(video_window != NULL);
  CHECK(data != NULL);
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  CHECK_GE(stride, width);
  height = (height + 15) & ~15;
  bool success = true;
  uint8* y_start = reinterpret_cast<uint8*>(data);
  uint8* u_start = y_start + height * stride * 5 / 4;
  uint8* v_start = y_start + height * stride;
  static uint8* rgb_frame = new uint8[height * stride * 4];
  int y_stride = stride;
  int uv_stride = stride / 2;
  int rgb_stride = stride * 4;
  ConvertYUVToRGB32(y_start, u_start, v_start, rgb_frame,
                    width, height, y_stride, uv_stride,
                    rgb_stride, YV12);
  PAINTSTRUCT ps;
  InvalidateRect(video_window, NULL, TRUE);
  HDC hdc = BeginPaint(video_window, &ps);
  BITMAPINFOHEADER hdr;
  hdr.biSize = sizeof(BITMAPINFOHEADER);
  hdr.biWidth = width;
  hdr.biHeight = -height;  // minus means top-down bitmap
  hdr.biPlanes = 1;
  hdr.biBitCount = 32;
  hdr.biCompression = BI_RGB;  // no compression
  hdr.biSizeImage = 0;
  hdr.biXPelsPerMeter = 1;
  hdr.biYPelsPerMeter = 1;
  hdr.biClrUsed = 0;
  hdr.biClrImportant = 0;
  int rv = StretchDIBits(hdc, 0, 0, width, height, 0, 0, width, height,
                         rgb_frame, reinterpret_cast<BITMAPINFO*>(&hdr),
                         DIB_RGB_COLORS, SRCCOPY);
  if (rv == 0) {
    LOG(ERROR) << "StretchDIBits failed";
    MessageLoopForUI::current()->QuitNow();
    success = false;
  }
  EndPaint(video_window, &ps);

  return success;
}

// Obtains the underlying raw data buffer for the given IMFMediaBuffer, and
// calls ConvertToRGBAndDrawToWindow() with it.
// Returns: true on success.
bool PaintMediaBufferOntoWindow(HWND video_window, IMFMediaBuffer* video_buffer,
                                int width, int height, int stride) {
  CHECK(video_buffer != NULL);
  HRESULT hr;
  BYTE* data;
  DWORD buffer_length;
  DWORD data_length;
  hr = video_buffer->Lock(&data, &buffer_length, &data_length);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to lock IMFMediaBuffer";
    return false;
  }
  if (!ConvertToRGBAndDrawToWindow(video_window,
                                   reinterpret_cast<uint8*>(data),
                                   width,
                                   height,
                                   stride)) {
    LOG(ERROR) << "Failed to convert raw buffer to RGB and draw to window";
    video_buffer->Unlock();
    return false;
  }
  video_buffer->Unlock();
  return true;
}

// Obtains the D3D9 surface from the given IMFMediaBuffer, then calls methods
// in the D3D device to draw to the window associated with it.
// Returns: true on success.
bool PaintD3D9BufferOntoWindow(IDirect3DDevice9* device,
                               IMFMediaBuffer* video_buffer) {
  CHECK(device != NULL);
  ScopedComPtr<IDirect3DSurface9> surface;
  HRESULT hr = MFGetService(video_buffer, MR_BUFFER_SERVICE,
                            IID_PPV_ARGS(surface.Receive()));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get D3D9 surface from buffer";
    return false;
  }
  hr = device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0),
                     1.0f, 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "Device->Clear() failed";
    return false;
  }
  ScopedComPtr<IDirect3DSurface9> backbuffer;
  hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
                             backbuffer.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Device->GetBackBuffer() failed";
    return false;
  }
  hr = device->StretchRect(surface.get(), NULL, backbuffer.get(), NULL,
                           D3DTEXF_NONE);
  if (FAILED(hr)) {
    LOG(ERROR) << "Device->StretchRect() failed";
    return false;
  }
  hr = device->Present(NULL, NULL, NULL, NULL);
  if (FAILED(hr)) {
    if (hr == E_FAIL) {
      LOG(WARNING) << "Present() returned E_FAIL";
    } else {
      static int frames_dropped = 0;
      LOG(ERROR) << "Device->Present() failed "
                 << std::hex << std::showbase << hr;
      if (++frames_dropped == 10) {
        LOG(ERROR) << "Dropped too many frames, quitting";
        MessageLoopForUI::current()->QuitNow();
        return false;
      }
    }
  }
  return true;
}

static void ReleaseOutputBuffer(VideoFrame* frame) {
  if (frame != NULL &&
      frame->type() == VideoFrame::TYPE_MFBUFFER ||
      frame->type() == VideoFrame::TYPE_DIRECT3DSURFACE) {
    static_cast<IMFMediaBuffer*>(frame->private_buffer())->Release();
  }
}

// NullRenderer

NullRenderer::NullRenderer(MftH264Decoder* decoder) : MftRenderer(decoder) {}
NullRenderer::~NullRenderer() {}

void NullRenderer::ProcessFrame(scoped_refptr<VideoFrame> frame) {
    ReleaseOutputBuffer(frame);
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(decoder_.get(),
                                     &MftH264Decoder::GetOutput));
}

void NullRenderer::StartPlayback() {
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(decoder_.get(),
                                   &MftH264Decoder::GetOutput));
}

void NullRenderer::StopPlayback() {
  MessageLoop::current()->Quit();
}

// BasicRenderer

BasicRenderer::BasicRenderer(MftH264Decoder* decoder,
                             HWND window, IDirect3DDevice9* device)
    : MftRenderer(decoder),
      window_(window),
      device_(device) {
}

BasicRenderer::~BasicRenderer() {}

void BasicRenderer::ProcessFrame(scoped_refptr<VideoFrame> frame) {
  if (device_ != NULL) {
    if (!PaintD3D9BufferOntoWindow(device_,
             static_cast<IMFMediaBuffer*>(frame->private_buffer()))) {
      MessageLoopForUI::current()->QuitNow();
    }
  } else {
    if (!PaintMediaBufferOntoWindow(
             window_, static_cast<IMFMediaBuffer*>(frame->private_buffer()),
             frame->width(), frame->height(), frame->stride(0))) {
      MessageLoopForUI::current()->QuitNow();
    }
  }
  ReleaseOutputBuffer(frame);
  MessageLoopForUI::current()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(decoder_.get(),
                                   &MftH264Decoder::GetOutput),
                                   frame->GetDuration().InMilliseconds());
}

void BasicRenderer::StartPlayback() {
  MessageLoopForUI::current()->PostTask(
      FROM_HERE, NewRunnableMethod(decoder_.get(),
                                   &MftH264Decoder::GetOutput));
}

void BasicRenderer::StopPlayback() {
  MessageLoopForUI::current()->Quit();
}

}  // namespace media
