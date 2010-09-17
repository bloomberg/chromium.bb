// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mf/mft_h264_decoder.h"

#include <d3d9.h>
#include <dxva2api.h>
#include <initguid.h>
#include <mfapi.h>
// Placed after mfapi.h to avoid linking strmiids.lib for MR_BUFFER_SERVICE.
#include <evr.h>
#include <mferror.h>
#include <wmcodecdsp.h>

#include "base/time.h"
#include "base/message_loop.h"

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")

using base::TimeDelta;

namespace {

// Creates an empty Media Foundation sample with no buffers.
static IMFSample* CreateEmptySample() {
  HRESULT hr;
  ScopedComPtr<IMFSample> sample;
  hr = MFCreateSample(sample.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create an empty sample";
    return NULL;
  }
  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer of length |buffer_length|
// on a |align|-byte boundary. Alignment must be a perfect power of 2 or 0.
// If |align| is 0, then no alignment is specified.
static IMFSample* CreateEmptySampleWithBuffer(int buffer_length, int align) {
  CHECK_GT(buffer_length, 0);
  ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySample());
  if (!sample.get())
    return NULL;
  ScopedComPtr<IMFMediaBuffer> buffer;
  HRESULT hr;
  if (align == 0) {
    // Note that MFCreateMemoryBuffer is same as MFCreateAlignedMemoryBuffer
    // with the align argument being 0.
    hr = MFCreateMemoryBuffer(buffer_length, buffer.Receive());
  } else {
    hr = MFCreateAlignedMemoryBuffer(buffer_length,
                                     align - 1,
                                     buffer.Receive());
  }
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create an empty buffer";
    return NULL;
  }
  hr = sample->AddBuffer(buffer.get());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to add empty buffer to sample";
    return NULL;
  }
  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample time are not known, provide 0.
// |min_size| specifies the minimum size of the buffer (might be required by
// the decoder for input). The times here should be given in 100ns units.
// |alignment| specifies the buffer in the sample to be aligned. If no
// alignment is required, provide 0 or 1.
static IMFSample* CreateInputSample(const uint8* stream, int size,
                                    int64 timestamp, int64 duration,
                                    int min_size, int alignment) {
  CHECK(stream);
  CHECK_GT(size, 0);
  ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySampleWithBuffer(std::max(min_size, size),
                                            alignment));
  if (!sample.get()) {
    LOG(ERROR) << "Failed to create empty buffer for input";
    return NULL;
  }
  HRESULT hr;
  if (duration > 0) {
    hr = sample->SetSampleDuration(duration);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to set sample duration";
      return NULL;
    }
  }
  if (timestamp > 0) {
    hr = sample->SetSampleTime(timestamp);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to set sample time";
      return NULL;
    }
  }
  ScopedComPtr<IMFMediaBuffer> buffer;
  hr = sample->GetBufferByIndex(0, buffer.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get buffer in sample";
    return NULL;
  }
  DWORD max_length, current_length;
  uint8* destination;
  hr = buffer->Lock(&destination, &max_length, &current_length);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to lock buffer";
    return NULL;
  }
  CHECK_EQ(current_length, 0u);
  CHECK_GE(static_cast<int>(max_length), size);
  memcpy(destination, stream, size);
  CHECK(SUCCEEDED(buffer->Unlock()));
  hr = buffer->SetCurrentLength(size);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set current length to " << size;
    return NULL;
  }
  LOG(INFO) << __FUNCTION__ << " wrote " << size << " bytes into input sample";
  return sample.Detach();
}

const GUID ConvertVideoFrameFormatToGuid(media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::NV12:
      return MFVideoFormat_NV12;
    case media::VideoFrame::YV12:
      return MFVideoFormat_YV12;
    default:
      NOTREACHED() << "Unsupported VideoFrame format";
      return GUID_NULL;
  }
  NOTREACHED();
  return GUID_NULL;
}

}  // namespace

namespace media {

// public methods

MftH264Decoder::MftH264Decoder(bool use_dxva, HWND draw_window)
    : use_dxva_(use_dxva),
      d3d9_(NULL),
      device_(NULL),
      device_manager_(NULL),
      draw_window_(draw_window),
      decoder_(NULL),
      input_stream_info_(),
      output_stream_info_(),
      state_(kUninitialized),
      event_handler_(NULL) {
  memset(&config_, 0, sizeof(config_));
  memset(&info_, 0, sizeof(info_));
}

MftH264Decoder::~MftH264Decoder() {
}

void MftH264Decoder::Initialize(
    MessageLoop* message_loop,
    VideoDecodeEngine::EventHandler* event_handler,
    VideoDecodeContext* context,
    const VideoCodecConfig& config) {
  LOG(INFO) << "MftH264Decoder::Initialize";
  if (state_ != kUninitialized) {
    LOG(ERROR) << "Initialize: invalid state";
    return;
  }
  if (!message_loop || !event_handler) {
    LOG(ERROR) << "MftH264Decoder::Initialize: parameters cannot be NULL";
    return;
  }

  config_ = config;
  event_handler_ = event_handler;

  info_.provides_buffers = true;

  // TODO(jiesun): Actually it is more likely an NV12 D3DSuface9.
  // Until we had hardware composition working.
  if (use_dxva_) {
    info_.stream_info.surface_format = VideoFrame::NV12;
    info_.stream_info.surface_type = VideoFrame::TYPE_D3D_TEXTURE;
  } else {
    info_.stream_info.surface_format = VideoFrame::YV12;
    info_.stream_info.surface_type = VideoFrame::TYPE_SYSTEM_MEMORY;
  }

  // codec_info.stream_info_.surface_width_/height_ are initialized
  // in InitInternal().
  info_.success = InitInternal();
  if (info_.success) {
    state_ = kNormal;
    event_handler_->OnInitializeComplete(info_);
  } else {
    LOG(ERROR) << "MftH264Decoder::Initialize failed";
  }
}

void MftH264Decoder::Uninitialize() {
  LOG(INFO) << "MftH264Decoder::Uninitialize";
  if (state_ == kUninitialized) {
    LOG(ERROR) << "Uninitialize: invalid state";
    return;
  }

  // TODO(imcheng):
  // Cannot shutdown COM libraries here because the COM objects still needs
  // to be Release()'ed. We can explicitly release them here, or move the
  // uninitialize to GpuVideoService...
  decoder_.Release();
  device_manager_.Release();
  device_.Release();
  d3d9_.Release();
  ShutdownComLibraries();
  state_ = kUninitialized;
  event_handler_->OnUninitializeComplete();
}

void MftH264Decoder::Flush() {
  LOG(INFO) << "MftH264Decoder::Flush";
  if (state_ != kNormal) {
    LOG(ERROR) << "Flush: invalid state";
    return;
  }
  state_ = kFlushing;
  if (!SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH)) {
    LOG(WARNING) << "MftH264Decoder::Flush failed to send message";
  }
  state_ = kNormal;
  event_handler_->OnFlushComplete();
}

void MftH264Decoder::Seek() {
  if (state_ != kNormal) {
    LOG(ERROR) << "Seek: invalid state";
    return;
  }
  LOG(INFO) << "MftH264Decoder::Seek";
  // Seek not implemented.
  event_handler_->OnSeekComplete();
}

void MftH264Decoder::ConsumeVideoSample(scoped_refptr<Buffer> buffer) {
  LOG(INFO) << "MftH264Decoder::ConsumeVideoSample";
  if (state_ == kUninitialized) {
    LOG(ERROR) << "ConsumeVideoSample: invalid state";
  }
  ScopedComPtr<IMFSample> sample;
  if (!buffer->IsEndOfStream()) {
    sample.Attach(
        CreateInputSample(buffer->GetData(),
                          buffer->GetDataSize(),
                          buffer->GetTimestamp().InMicroseconds() * 10,
                          buffer->GetDuration().InMicroseconds() * 10,
                          input_stream_info_.cbSize,
                          input_stream_info_.cbAlignment));
    if (!sample.get()) {
      LOG(ERROR) << "Failed to create an input sample";
    } else {
      if (FAILED(decoder_->ProcessInput(0, sample.get(), 0))) {
        event_handler_->OnError();
      }
    }
  } else {
    if (state_ != MftH264Decoder::kEosDrain) {
      // End of stream, send drain messages.
      if (!SendMFTMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM) ||
          !SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN)) {
        LOG(ERROR) << "Failed to send EOS / drain messages to MFT";
        event_handler_->OnError();
      } else {
        state_ = MftH264Decoder::kEosDrain;
      }
    }
  }
  DoDecode();
}

void MftH264Decoder::ProduceVideoFrame(scoped_refptr<VideoFrame> frame) {
  LOG(INFO) << "MftH264Decoder::ProduceVideoFrame";
  if (state_ == kUninitialized) {
    LOG(ERROR) << "ProduceVideoFrame: invalid state";
    return;
  }
  scoped_refptr<Buffer> buffer;
  event_handler_->ProduceVideoSample(buffer);
}

// private methods

// static
bool MftH264Decoder::StartupComLibraries() {
  HRESULT hr;
  hr = CoInitializeEx(NULL,
                      COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    LOG(ERROR) << "CoInit fail";
    return false;
  }

  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "MFStartup fail";
    CoUninitialize();
    return false;
  }
  return true;
}

// static
void MftH264Decoder::ShutdownComLibraries() {
  HRESULT hr;
  hr = MFShutdown();
  if (FAILED(hr)) {
    LOG(WARNING) << "Warning: MF failed to shutdown";
  }
  CoUninitialize();
}

bool MftH264Decoder::CreateD3DDevManager() {
  CHECK(draw_window_);
  d3d9_.Attach(Direct3DCreate9(D3D_SDK_VERSION));
  if (d3d9_.get() == NULL) {
    LOG(ERROR) << "Failed to create D3D9";
    return false;
  }

  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = 0;
  present_params.BackBufferHeight = 0;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = draw_window_;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  // D3DCREATE_HARDWARE_VERTEXPROCESSING specifies hardware vertex processing.
  // (Is it even needed for just video decoding?)
  HRESULT hr = d3d9_->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   draw_window_,
                                   (D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                    D3DCREATE_MULTITHREADED),
                                   &present_params,
                                   device_.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D3D Device";
    return false;
  }

  UINT dev_manager_reset_token = 0;
  hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token,
                                         device_manager_.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Couldn't create D3D Device manager";
    return false;
  }

  hr = device_manager_->ResetDevice(device_.get(),
                                    dev_manager_reset_token);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set device to device manager";
    return false;
  }
  return true;
}

bool MftH264Decoder::InitInternal() {
  if (!StartupComLibraries())
    return false;
  if (use_dxva_ && !CreateD3DDevManager())
    return false;
  if (!InitDecoder())
    return false;
  if (!GetStreamsInfoAndBufferReqs())
    return false;
  return SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING);
}

bool MftH264Decoder::InitDecoder() {
  // TODO(jiesun): use MFEnum to get decoder CLSID.
  HRESULT hr = CoCreateInstance(__uuidof(CMSH264DecoderMFT),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMFTransform),
                                reinterpret_cast<void**>(decoder_.Receive()));
  if (FAILED(hr) || !decoder_.get()) {
    LOG(ERROR) << "CoCreateInstance failed " << std::hex << std::showbase << hr;
    return false;
  }

  if (!CheckDecoderDxvaSupport())
    return false;

  if (use_dxva_) {
    hr = decoder_->ProcessMessage(
        MFT_MESSAGE_SET_D3D_MANAGER,
        reinterpret_cast<ULONG_PTR>(device_manager_.get()));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to set D3D9 device to decoder " << std::hex << hr;
      return false;
    }
  }

  return SetDecoderMediaTypes();
}

bool MftH264Decoder::CheckDecoderDxvaSupport() {
  ScopedComPtr<IMFAttributes> attributes;
  HRESULT hr = decoder_->GetAttributes(attributes.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unlock: Failed to get attributes, hr = "
               << std::hex << std::showbase << hr;
    return false;
  }

  UINT32 dxva;
  hr = attributes->GetUINT32(MF_SA_D3D_AWARE, &dxva);
  if (FAILED(hr) || !dxva) {
    LOG(ERROR) << "Failed to get DXVA attr or decoder is not DXVA-aware, hr = "
               << std::hex << std::showbase << hr
               << " this might not be the right decoder.";
    return false;
  }
  return true;
}

bool MftH264Decoder::SetDecoderMediaTypes() {
  if (!SetDecoderInputMediaType())
    return false;
  return SetDecoderOutputMediaType(ConvertVideoFrameFormatToGuid(
                                       info_.stream_info.surface_format));
}

bool MftH264Decoder::SetDecoderInputMediaType() {
  ScopedComPtr<IMFMediaType> media_type;
  HRESULT hr = MFCreateMediaType(media_type.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create empty media type object";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    LOG(ERROR) << "SetGUID for major type failed";
    return false;
  }

  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  if (FAILED(hr)) {
    LOG(ERROR) << "SetGUID for subtype failed";
    return false;
  }

  hr = decoder_->SetInputType(0, media_type.get(), 0);  // No flags
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set decoder's input type";
    return false;
  }

  return true;
}

bool MftH264Decoder::SetDecoderOutputMediaType(const GUID subtype) {
  DWORD i = 0;
  IMFMediaType* out_media_type;
  bool found = false;
  while (SUCCEEDED(decoder_->GetOutputAvailableType(0, i, &out_media_type))) {
    GUID out_subtype;
    HRESULT hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to GetGUID() on GetOutputAvailableType() " << i;
      out_media_type->Release();
      continue;
    }
    if (out_subtype == subtype) {
      hr = decoder_->SetOutputType(0, out_media_type, 0);  // No flags
      hr = MFGetAttributeSize(out_media_type, MF_MT_FRAME_SIZE,
          reinterpret_cast<UINT32*>(&info_.stream_info.surface_width),
          reinterpret_cast<UINT32*>(&info_.stream_info.surface_height));
      config_.width = info_.stream_info.surface_width;
      config_.height = info_.stream_info.surface_height;
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to SetOutputType to |subtype| or obtain "
                   << "width/height " << std::hex << hr;
      } else {
        out_media_type->Release();
        return true;
      }
    }
    i++;
    out_media_type->Release();
  }
  return false;
}

bool MftH264Decoder::SendMFTMessage(MFT_MESSAGE_TYPE msg) {
  HRESULT hr = decoder_->ProcessMessage(msg, NULL);
  return SUCCEEDED(hr);
}

// Prints out info about the input/output streams, gets the minimum buffer sizes
// for input and output samples.
// The MFT will not allocate buffer for neither input nor output, so we have
// to do it ourselves and make sure they're the correct size.
// Exception is when dxva is enabled, the decoder will allocate output.
bool MftH264Decoder::GetStreamsInfoAndBufferReqs() {
  HRESULT hr = decoder_->GetInputStreamInfo(0, &input_stream_info_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get input stream info";
    return false;
  }
  LOG(INFO) << "Input stream info: ";
  LOG(INFO) << "Max latency: " << input_stream_info_.hnsMaxLatency;

  // There should be three flags, one for requiring a whole frame be in a
  // single sample, one for requiring there be one buffer only in a single
  // sample, and one that specifies a fixed sample size. (as in cbSize)
  LOG(INFO) << "Flags: "
            << std::hex << std::showbase << input_stream_info_.dwFlags;
  CHECK_EQ(input_stream_info_.dwFlags, 0x7u);
  LOG(INFO) << "Min buffer size: " << input_stream_info_.cbSize;
  LOG(INFO) << "Max lookahead: " << input_stream_info_.cbMaxLookahead;
  LOG(INFO) << "Alignment: " << input_stream_info_.cbAlignment;

  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get output stream info";
    return false;
  }
  LOG(INFO) << "Output stream info: ";
  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  LOG(INFO) << "Flags: "
            << std::hex << std::showbase << output_stream_info_.dwFlags;
  CHECK_EQ(output_stream_info_.dwFlags, use_dxva_ ? 0x107u : 0x7u);
  LOG(INFO) << "Min buffer size: " << output_stream_info_.cbSize;
  LOG(INFO) << "Alignment: " << output_stream_info_.cbAlignment;

  return true;
}

bool MftH264Decoder::DoDecode() {
  if (state_ != kNormal && state_ != kEosDrain) {
    LOG(ERROR) << "DoDecode: not in normal or drain state";
    return false;
  }
  scoped_refptr<VideoFrame> frame;
  ScopedComPtr<IMFSample> output_sample;
  if (!use_dxva_) {
    output_sample.Attach(
        CreateEmptySampleWithBuffer(output_stream_info_.cbSize,
                                    output_stream_info_.cbAlignment));
    if (!output_sample.get()) {
      LOG(ERROR) << "GetSample: failed to create empty output sample";
      event_handler_->OnError();
      return false;
    }
  }
  MFT_OUTPUT_DATA_BUFFER output_data_buffer;
  memset(&output_data_buffer, 0, sizeof(output_data_buffer));
  output_data_buffer.dwStreamID = 0;
  output_data_buffer.pSample = output_sample;

  DWORD status;
  HRESULT hr = decoder_->ProcessOutput(0,  // No flags
                                       1,  // # of out streams to pull from
                                       &output_data_buffer,
                                       &status);

  IMFCollection* events = output_data_buffer.pEvents;
  if (events != NULL) {
    LOG(INFO) << "Got events from ProcessOuput, but discarding";
    events->Release();
  }

  if (FAILED(hr)) {
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      hr = SetDecoderOutputMediaType(ConvertVideoFrameFormatToGuid(
                                         info_.stream_info.surface_format));
      if (SUCCEEDED(hr)) {
        event_handler_->OnFormatChange(info_.stream_info);
        return true;
      } else {
        event_handler_->OnError();
        return false;
      }
    } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      if (state_ == kEosDrain) {
        // No more output from the decoder. Notify EOS and stop playback.
        scoped_refptr<VideoFrame> frame;
        VideoFrame::CreateEmptyFrame(&frame);
        event_handler_->ConsumeVideoFrame(frame);
        state_ = MftH264Decoder::kStopped;
        return false;
      }
      return true;
    } else {
      LOG(ERROR) << "Unhandled error in DoDecode()";
      state_ = MftH264Decoder::kStopped;
      event_handler_->OnError();
      return false;
    }
  }

  // We succeeded in getting an output sample.
  if (use_dxva_) {
    // For DXVA we didn't provide the sample, i.e. output_sample was NULL.
    output_sample.Attach(output_data_buffer.pSample);
  }
  if (!output_sample.get()) {
    LOG(ERROR) << "ProcessOutput succeeded, but did not get a sample back";
    event_handler_->OnError();
    return true;
  }

  int64 timestamp = 0, duration = 0;
  if (FAILED(output_sample->GetSampleTime(&timestamp)) ||
      FAILED(output_sample->GetSampleDuration(&duration))) {
    LOG(WARNING) << "Failed to get timestamp/duration from output";
  }

  // The duration and timestamps are in 100-ns units, so divide by 10
  // to convert to microseconds.
  timestamp /= 10;
  duration /= 10;

  // Sanity checks for checking if there is really something in the sample.
  DWORD buf_count;
  hr = output_sample->GetBufferCount(&buf_count);
  if (FAILED(hr) || buf_count != 1) {
    LOG(ERROR) << "Failed to get buffer count, or buffer count mismatch";
    return true;
  }

  ScopedComPtr<IMFMediaBuffer> output_buffer;
  hr = output_sample->GetBufferByIndex(0, output_buffer.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get buffer from sample";
    return true;
  }



  if (use_dxva_) {
    ScopedComPtr<IDirect3DSurface9> surface;
    hr = MFGetService(output_buffer, MR_BUFFER_SERVICE,
                      IID_PPV_ARGS(surface.Receive()));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get surface from buffer";
      return true;
    }

    // No distinction between the 3 planes - all 3 point to the handle of
    // the texture. (There are actually only 2 planes since the output
    // D3D surface is in NV12 format.)
    VideoFrame::D3dTexture textures[VideoFrame::kMaxPlanes] = { surface.get(),
                                                                surface.get(),
                                                                surface.get() };
    VideoFrame::CreateFrameD3dTexture(info_.stream_info.surface_format,
                                      info_.stream_info.surface_width,
                                      info_.stream_info.surface_height,
                                      textures,
                                      TimeDelta::FromMicroseconds(timestamp),
                                      TimeDelta::FromMicroseconds(duration),
                                      &frame);
  if (!frame.get()) {
    LOG(ERROR) << "Failed to allocate video frame for d3d texture";
    event_handler_->OnError();
    return true;
  }

  // The reference is now in the VideoFrame.
  surface.Detach();
  } else {
    // Not DXVA.
    VideoFrame::CreateFrame(info_.stream_info.surface_format,
                            info_.stream_info.surface_width,
                            info_.stream_info.surface_height,
                            TimeDelta::FromMicroseconds(timestamp),
                            TimeDelta::FromMicroseconds(duration),
                            &frame);
    if (!frame.get()) {
      LOG(ERROR) << "Failed to allocate video frame for yuv plane";
      event_handler_->OnError();
      return true;
    }
    uint8* src_y;
    DWORD max_length, current_length;
    HRESULT hr = output_buffer->Lock(&src_y, &max_length, &current_length);
    if (FAILED(hr))
      return true;
    uint8* dst_y = static_cast<uint8*>(frame->data(VideoFrame::kYPlane));

    memcpy(dst_y, src_y, current_length);
    CHECK(SUCCEEDED(output_buffer->Unlock()));
  }
  // TODO(jiesun): non-System memory case
  event_handler_->ConsumeVideoFrame(frame);
  return true;
}

}  // namespace media
