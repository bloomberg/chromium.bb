// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNICODE
#undef UNICODE
#endif

#include <algorithm>

#include <d3d9.h>
#include <dxva2api.h>
#include <evr.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <windows.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "media/base/yuv_convert.h"
#include "media/tools/mfdecoder/mfdecoder.h"
#include "ui/gfx/gdi_util.h"

namespace {

const char* const kWindowClass = "Chrome_MF_Decoder";
const char* const kWindowTitle = "MF Decoder";
const int kWindowStyleFlags = (WS_OVERLAPPEDWINDOW | WS_VISIBLE) &
                              ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
bool g_render_to_window = false;
bool g_render_asap = false;

base::TimeDelta* g_decode_time;
base::TimeDelta* g_render_time;
int64 g_num_frames = 0;

void usage() {
  static char* usage_msg = "Usage: mfdecoder (-s|-h) (-d|-r|-f) input-file\n"
                           "-s: Use software decoding\n"
                           "-h: Use hardware decoding\n"
                           "\n"
                           "-d: Decode to YV12 as fast as possible, no " \
                           "rendering or color-space conversion\n"
                           "-r: Render to window at 30ms per frame\n"
                           "-f: Decode and render as fast as possible\n"
                           "\n"
                           "To see this message: mfdecoder --help\n";
  fprintf(stderr, "%s", usage_msg);
}

// Converts an ASCII string to an Unicode string. This function allocates
// space for the returned Unicode string from the heap and it is caller's
// responsibility to free it.
// Returns: An equivalent Unicode string if successful, NULL otherwise.
wchar_t* ConvertASCIIStringToUnicode(const char* source) {
  if (source == NULL) {
    LOG(ERROR) << "ConvertASCIIStringToUnicode: source cannot be NULL";
    return NULL;
  }
  DWORD string_length = MultiByteToWideChar(CP_ACP, 0, source, -1, NULL, 0);
  if (string_length == 0) {
    LOG(ERROR) << "Error getting size of ansi string";
    return NULL;
  }
  scoped_array<wchar_t> ret(new wchar_t[string_length]);
  if (ret.get() == NULL) {
    LOG(ERROR) << "Error allocating unicode string buffer";
    return NULL;
  }
  if (MultiByteToWideChar(CP_ACP, 0, source, string_length, ret.get(),
                          string_length) == 0) {
    LOG(ERROR) << "Error converting ansi string to unicode";
    return NULL;
  }
  return ret.release();
}

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
  media::ConvertYUVToRGB32(y_start, u_start, v_start, rgb_frame,
                           width, height, y_stride, uv_stride,
                           rgb_stride, media::YV12);

  PAINTSTRUCT ps;
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
  if (g_render_to_window) {
    base::Time render_start(base::Time::Now());
    if (!ConvertToRGBAndDrawToWindow(video_window,
                                     reinterpret_cast<uint8*>(data),
                                     width,
                                     height,
                                     stride)) {
      LOG(ERROR) << "Failed to convert raw buffer to RGB and draw to window";
      video_buffer->Unlock();
      return false;
    }
    *g_render_time += base::Time::Now() - render_start;
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
  if (g_render_to_window) {
    base::Time render_start(base::Time::Now());
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
    if (FAILED(hr) && hr != E_FAIL) {
      static int frames_dropped = 0;
      LOG(ERROR) << "Device->Present() failed "
                 << std::hex << std::showbase << hr;
      if (++frames_dropped == 10) {
        LOG(ERROR) << "Dropped too many frames, quitting";
        MessageLoopForUI::current()->Quit();
        return false;
      }
    }
    *g_render_time += base::Time::Now() - render_start;
  }
  return true;
}

// Reads a sample from the given decoder, and draws the sample to the given
// window. Obtains the IMFMediaBuffer objects from the given IMFSample, and
// calls either PaintMediaBufferOntoWindow() or PaintD3D9BufferOntoWindow() with
// each of them, depending on whether the decoder supports DXVA2.
// The decoder should be initialized before calling this method.
// For H.264 format, there should only be 1 buffer per sample, so each buffer
// represents 1 frame.
// Returns: true if successful.
bool DrawVideoSample(HWND video_window, media::MFDecoder* decoder,
                     IDirect3DDevice9* device) {
  CHECK(video_window != NULL);
  CHECK(decoder != NULL);
  CHECK(decoder->initialized());

  if (decoder->end_of_stream()) {
    LOG(ERROR) << "Failed to obtain more samples from decoder because end of "
               << "stream has been reached";
    return false;
  }
  ScopedComPtr<IMFSample> video_sample;
  base::Time decode_time_start(base::Time::Now());
  video_sample.Attach(decoder->ReadVideoSample());
  if (video_sample.get() == NULL) {
    LOG(ERROR) << "Failed to obtain a sample from decoder: end of stream? "
               << (decoder->end_of_stream() ? "true" : "false");
    return false;
  }
  *g_decode_time += base::Time::Now() - decode_time_start;

  // Get the buffer inside the sample.
  DWORD buffer_count;
  HRESULT hr = video_sample->GetBufferCount(&buffer_count);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get buffer count from sample";
    return false;
  }

  // For H.264 videos, the number of buffers in the sample is 1.
  CHECK_EQ(buffer_count, 1u) << "buffer_count should be equal "
                                              << "to 1 for H.264 format";
  ScopedComPtr<IMFMediaBuffer> video_buffer;
  hr = video_sample->GetBufferByIndex(0, video_buffer.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get buffer from sample";
    return false;
  }
  if (decoder->use_dxva2()) {
    return PaintD3D9BufferOntoWindow(device, video_buffer);
  } else {
    return PaintMediaBufferOntoWindow(video_window, video_buffer,
                                      decoder->width(), decoder->height(),
                                      decoder->mfbuffer_stride());
  }
}

// Creates a window with the given width and height.
// Returns: A handle to the window on success, NULL otherwise.
HWND CreateDrawWindow(int width, int height) {
  WNDCLASS window_class = {0};
  window_class.lpszClassName = kWindowClass;
  window_class.hInstance = NULL;
  window_class.hbrBackground = 0;
  window_class.lpfnWndProc = DefWindowProc;
  window_class.hCursor = LoadCursor(0, IDC_ARROW);

  if (RegisterClass(&window_class) == 0) {
    LOG(ERROR) << "Failed to register window class";
    return false;
  }
  HWND window = CreateWindow(kWindowClass,
                             kWindowTitle,
                             kWindowStyleFlags,
                             100,
                             100,
                             width+100,
                             height+30,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
  if (window == NULL) {
    LOG(ERROR) << "Failed to create window";
    return NULL;
  }
  return window;
}

// This function creates a D3D Device and a D3D Device Manager, sets the manager
// to use the device, and returns the manager. It also initializes the D3D
// device. This function is used by mfdecoder.cc during the call to
// MFDecoder::GetDXVA2AttributesForSourceReader().
// Returns: The D3D manager object if successful. Otherwise, NULL is returned.
IDirect3DDeviceManager9* CreateD3DDevManager(HWND video_window,
                                             IDirect3DDevice9** device) {
  CHECK(video_window != NULL);
  CHECK(device != NULL);
  int ret = -1;

  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  ScopedComPtr<IDirect3D9> d3d;
  d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
  if (d3d == NULL) {
    LOG(ERROR) << "Failed to create D3D9";
    return NULL;
  }
  D3DPRESENT_PARAMETERS present_params = {0};

  // Not sure if these values are correct, or if
  // they even matter. (taken from DXVA_HD sample code)
  present_params.BackBufferWidth = 0;
  present_params.BackBufferHeight = 0;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = video_window;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  ScopedComPtr<IDirect3DDevice9> temp_device;

  // D3DCREATE_HARDWARE_VERTEXPROCESSING specifies hardware vertex processing.
  HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT,
                                 D3DDEVTYPE_HAL,
                                 video_window,
                                 (D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                  D3DCREATE_MULTITHREADED),
                                 &present_params,
                                 temp_device.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D3D Device";
    return NULL;
  }
  UINT dev_manager_reset_token = 0;
  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token,
                                         dev_manager.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Couldn't create D3D Device manager";
    return NULL;
  }
  hr = dev_manager->ResetDevice(temp_device.get(), dev_manager_reset_token);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set device to device manager";
    return NULL;
  }
  *device = temp_device.Detach();
  return dev_manager.Detach();
}

// Resets the D3D device to prevent scaling from happening because it was
// created with window before resizing occurred. We need to change the back
// buffer dimensions to the actual video frame dimensions.
// Both the decoder and device should be initialized before calling this method.
// Returns: true if successful.
bool AdjustD3DDeviceBackBufferDimensions(media::MFDecoder* decoder,
                                         IDirect3DDevice9* device,
                                         HWND video_window) {
  CHECK(decoder != NULL);
  CHECK(decoder->initialized());
  CHECK(decoder->use_dxva2());
  CHECK(device != NULL);
  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = decoder->width();
  present_params.BackBufferHeight = decoder->height();
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = video_window;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  return SUCCEEDED(device->Reset(&present_params)) ? true : false;
}

// Post this task in the MessageLoop. This function keeps posting itself
// until DrawVideoSample fails.
void RepaintTask(media::MFDecoder* decoder, HWND video_window,
                 IDirect3DDevice9* device) {
  // This sends a WM_PAINT message so we can paint on the window later.
  // If we are using D3D9, then we do not send a WM_PAINT message since the two
  // do not work well together.
  if (!decoder->use_dxva2())
    InvalidateRect(video_window, NULL, TRUE);
  base::Time start(base::Time::Now());
  if (!DrawVideoSample(video_window, decoder, device)) {
    LOG(ERROR) << "DrawVideoSample failed, quitting MessageLoop";
    MessageLoopForUI::current()->Quit();
  } else {
    ++g_num_frames;
    if (!g_render_asap && g_render_to_window) {
      base::Time end(base::Time::Now());
      int64 delta = (end-start).InMilliseconds();
      MessageLoopForUI::current()->PostDelayedTask(
          FROM_HERE,
          NewRunnableFunction(&RepaintTask, decoder, video_window, device),
                              std::max<int64>(0L, 30-delta));
    } else {
      MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          NewRunnableFunction(&RepaintTask, decoder, video_window, device));
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "missing arguments\n");
    usage();
    return -1;
  }
  if (strcmp(argv[1], "--help") == 0) {
    usage();
    return 0;
  }
  if (argc != 4) {
    fprintf(stderr, "invalid number of arguments\n");
    usage();
    return -1;
  }

  bool use_dxva2 = false;
  if (strcmp(argv[1], "-s") == 0) {
    use_dxva2 = false;
  } else if (strcmp(argv[1], "-h") == 0) {
    use_dxva2 = true;
  } else {
    fprintf(stderr, "unknown option %s\n", argv[1]);
    usage();
    return -1;
  }
  VLOG(1) << "use_dxva2: " << use_dxva2;

  g_render_to_window = false;
  g_render_asap = false;
  if (strcmp(argv[2], "-d") == 0) {
    g_render_to_window = false;
  } else if (strcmp(argv[2], "-r") == 0) {
    g_render_to_window = true;
  } else if (strcmp(argv[2], "-f") == 0) {
    g_render_to_window = true;
    g_render_asap = true;
  } else {
    fprintf(stderr, "unknown option %s\n", argv[2]);
    usage();
    return -1;
  }
  VLOG(1) << "g_render_to_window: " << g_render_to_window
          << "\ng_render_asap: " << g_render_asap;

  scoped_array<wchar_t> file_name(ConvertASCIIStringToUnicode(argv[argc-1]));
  if (file_name.get() == NULL) {
    LOG(ERROR) << "Failed to convert file name to unicode";
    return -1;
  }

  // Once we initialized the decoder, we should resize the window to frame size.
  // For now, just create a window with arbitrary dimensions.
  HWND video_window = g_render_to_window ? CreateDrawWindow(640, 480)
                                         : GetDesktopWindow();
  if (video_window == NULL) {
    LOG(ERROR) << "main: Failed to create the video window";
    return -1;
  }
  scoped_ptr<media::MFDecoder> decoder(new media::MFDecoder(use_dxva2));
  if (decoder == NULL) {
    LOG(ERROR) << "Failed to create decoder";
    return -1;
  }
  ScopedComPtr<IDirect3DDeviceManager9> dev_manager;
  ScopedComPtr<IDirect3DDevice9> device;
  if (decoder->use_dxva2()) {
    dev_manager.Attach(CreateD3DDevManager(video_window, device.Receive()));
    if (dev_manager.get() == NULL || device.get() == NULL) {
      LOG(ERROR) << "DXVA2 specified, but failed to create D3D device";
      return -1;
    }
  }
  if (!decoder->Init(file_name.get(), dev_manager.get())) {
    LOG(ERROR) << "main: Decoder initialization failed";
    return -1;
  }

  // Resize the window to the dimensions of video frame.
  if (g_render_to_window) {
    RECT rect;
    rect.left = 0;
    rect.right = decoder->width();
    rect.top = 0;
    rect.bottom = decoder->height();
    AdjustWindowRect(&rect, kWindowStyleFlags, FALSE);
    if (!MoveWindow(video_window, 0, 0, rect.right - rect.left,
                    rect.bottom - rect.top, TRUE)) {
      LOG(WARNING) << "Warning: Failed to resize window";
    }
    if (decoder->use_dxva2()) {
      // Reset the device's back buffer dimensions to match the window's
      // dimensions.
      if (!AdjustD3DDeviceBackBufferDimensions(decoder.get(), device.get(),
                                               video_window)) {
        LOG(WARNING) << "Warning: Failed to reset device to have correct "
                     << "backbuffer dimension, scaling might occur";
      }
    }
  }
  g_decode_time = new base::TimeDelta();
  g_render_time = new base::TimeDelta();
  if (g_decode_time == NULL || g_render_time == NULL) {
    fprintf(stderr, "Failed to create decode/render timers\n");
    return -1;
  }
  base::Time start(base::Time::Now());
  printf("Decoding started\n");
  VLOG(1) << "Decoding " << file_name.get()
          << " started at " << start.ToTimeT();

  base::AtExitManager exit_manager;
  MessageLoopForUI message_loop;

  // The device is NULL if DXVA2 is not enabled.
  MessageLoopForUI::current()->PostTask(FROM_HERE,
                                        NewRunnableFunction(&RepaintTask,
                                                            decoder.get(),
                                                            video_window,
                                                            device.get()));
  MessageLoopForUI::current()->Run(NULL);

  printf("Decoding finished\n");
  base::Time end(base::Time::Now());
  VLOG(1) << "Decoding finished at " << end.ToTimeT()
          << "\nTook " << (end-start).InMilliseconds() << "ms"
          << "\nNumber of frames processed: " << g_num_frames
          << "\nDecode time: " << g_decode_time->InMilliseconds() << "ms"
          << "\nAverage decode time: " << ((g_num_frames == 0) ?
              0 : (g_decode_time->InMillisecondsF() / g_num_frames))
          << "\nRender time: " << g_render_time->InMilliseconds() << "ms"
          << "\nAverage render time: " << ((g_num_frames == 0) ?
              0 : (g_render_time->InMillisecondsF() / g_num_frames));
  printf("Normal termination\n");
  delete g_decode_time;
  delete g_render_time;
  return 0;
}
