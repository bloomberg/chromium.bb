// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_VIDEO_DECODER_MFT_H_
#define CHROME_GPU_GPU_VIDEO_DECODER_MFT_H_

#include "build/build_config.h"  // For OS_WIN.

#if defined(OS_WIN)

#include <d3d9.h>
#include <dxva2api.h>
#include <evr.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <shlwapi.h>
#include <wmcodecdsp.h>

#include <deque>

#include "base/scoped_comptr_win.h"
#include "chrome/gpu/gpu_video_decoder.h"

class GpuVideoDecoderMFT : public GpuVideoDecoder {
 public:
  virtual bool DoInitialize(const GpuVideoDecoderInitParam& init_param,
                            GpuVideoDecoderInitDoneParam* done_param);
  virtual bool DoUninitialize();
  virtual void DoFlush();
  virtual void DoEmptyThisBuffer(const GpuVideoDecoderInputBufferParam& buffer);
  virtual void DoFillThisBuffer(const GpuVideoDecoderOutputBufferParam& frame);
  virtual void DoFillThisBufferDoneACK();

 private:
  GpuVideoDecoderMFT(const GpuVideoDecoderInfoParam* param,
                     GpuChannel* channel_,
                     base::ProcessHandle handle);

  friend class GpuVideoService;

  // TODO(jiesun): Find a way to move all these to GpuVideoService..
  static bool StartupComLibraries();
  static void ShutdownComLibraries();
  bool CreateD3DDevManager(HWND video_window);

  // helper.
  bool InitMediaFoundation();
  bool InitDecoder();
  bool CheckDecoderDxvaSupport();

  bool SetDecoderMediaTypes();
  bool SetDecoderInputMediaType();
  bool SetDecoderOutputMediaType(const GUID subtype);
  bool SendMFTMessage(MFT_MESSAGE_TYPE msg);
  bool GetStreamsInfoAndBufferReqs();

  // Help function to create IMFSample* out of input buffer.
  // data are copied into IMFSample's own IMFMediaBuffer.
  // Client should Release() the IMFSample*.
  static IMFSample* CreateInputSample(uint8* data,
                                      int32 size,
                                      int64 timestamp,
                                      int64 duration,
                                      int32 min_size);

  bool DoDecode();

  ScopedComPtr<IDirect3D9> d3d9_;
  ScopedComPtr<IDirect3DDevice9> device_;
  ScopedComPtr<IDirect3DDeviceManager9> device_manager_;
  ScopedComPtr<IMFTransform> decoder_;

  MFT_INPUT_STREAM_INFO input_stream_info_;
  MFT_OUTPUT_STREAM_INFO output_stream_info_;

  std::deque<ScopedComPtr<IMFSample> > input_buffer_queue_;
  bool output_transfer_buffer_busy_;

  typedef enum {
    kNormal,     // normal playing state.
    kFlushing,   // upon received Flush(), before FlushDone()
    kEosFlush,   // upon input EOS received.
    kStopped,    // upon output EOS received.
  } State;
  State state_;

  int32 pending_request_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecoderMFT);
};

#endif

#endif  // CHROME_GPU_GPU_VIDEO_DECODER_MFT_H_

