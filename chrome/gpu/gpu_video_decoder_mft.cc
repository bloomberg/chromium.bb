// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_video_decoder_mft.h"

#if defined(OS_WIN)

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "strmiids.lib")

GpuVideoDecoderMFT::GpuVideoDecoderMFT(
    const GpuVideoDecoderInfoParam* param,
    GpuChannel* channel_,
    base::ProcessHandle handle)
    : GpuVideoDecoder(param, channel_, handle),
      state_(kNormal) {
  output_transfer_buffer_busy_ = false;
  pending_request_ = 0;
}

bool GpuVideoDecoderMFT::StartupComLibraries() {
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

void GpuVideoDecoderMFT::ShutdownComLibraries() {
  HRESULT hr;
  hr = MFShutdown();
  if (FAILED(hr)) {
    LOG(WARNING) << "Warning: MF failed to shutdown";
  }
  CoUninitialize();
}

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample_time are not known, provide 0.
// min_size specifies the minimum size of the buffer (might be required by
// the decoder for input). The times here should be given in 100ns units.
IMFSample* GpuVideoDecoderMFT::CreateInputSample(uint8* data,
                                                 int32 size,
                                                 int64 timestamp,
                                                 int64 duration,
                                                 int32 min_size) {
  ScopedComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(sample.Receive());
  if (FAILED(hr) || !sample.get()) {
    LOG(ERROR) << "Unable to create an empty sample";
    return NULL;
  }

  ScopedComPtr<IMFMediaBuffer> buffer;
  int32 buffer_length = min_size > size ? min_size : size;
  hr = MFCreateMemoryBuffer(buffer_length, buffer.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to create an empty buffer";
    return NULL;
  }

  hr = sample->AddBuffer(buffer.get());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to add empty buffer to sample";
    return NULL;
  }

  if (duration > 0 && FAILED(sample->SetSampleDuration(duration))) {
    LOG(ERROR) << "Failed to set sample duration";
    return NULL;
  }

  if (timestamp > 0 && FAILED(sample->SetSampleTime(timestamp))) {
    LOG(ERROR) << "Failed to set sample time";
    return NULL;
  }

  DWORD max_length, current_length;
  uint8* buffer_data;
  hr = buffer->Lock(&buffer_data, &max_length, &current_length);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to lock buffer";
    return NULL;
  }
  CHECK_GE(static_cast<int>(max_length), size);
  memcpy(buffer_data, data, size);
  CHECK(SUCCEEDED(buffer->Unlock()));

  hr = buffer->SetCurrentLength(size);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set current length to " << size;
    return NULL;
  }

  return sample.Detach();
}

bool GpuVideoDecoderMFT::CreateD3DDevManager(HWND video_window) {
  d3d9_.Attach(Direct3DCreate9(D3D_SDK_VERSION));
  if (d3d9_.get() == NULL) {
    LOG(ERROR) << "Failed to create D3D9";
    return false;
  }

  D3DPRESENT_PARAMETERS present_params = {0};
  present_params.BackBufferWidth = init_param_.width_;
  present_params.BackBufferHeight = init_param_.height_;
  present_params.BackBufferFormat = D3DFMT_UNKNOWN;
  present_params.BackBufferCount = 1;
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = video_window;
  present_params.Windowed = TRUE;
  present_params.Flags = D3DPRESENTFLAG_VIDEO;
  present_params.FullScreen_RefreshRateInHz = 0;
  present_params.PresentationInterval = 0;

  // D3DCREATE_HARDWARE_VERTEXPROCESSING specifies hardware vertex processing.
  // (Is it even needed for just video decoding?)
  HRESULT hr = d3d9_->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   video_window,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING,
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

bool GpuVideoDecoderMFT::InitMediaFoundation() {
  if (!StartupComLibraries())
    return false;

  if (!CreateD3DDevManager(GetDesktopWindow()))
    return false;

  if (!InitDecoder())
    return false;

  if (!GetStreamsInfoAndBufferReqs())
    return false;

  return SendMFTMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM);
}

bool GpuVideoDecoderMFT::InitDecoder() {
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

  hr = decoder_->ProcessMessage(
      MFT_MESSAGE_SET_D3D_MANAGER,
      reinterpret_cast<ULONG_PTR>(device_manager_.get()));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set D3D9 device to decoder";
    return false;
  }

  return SetDecoderMediaTypes();
}

bool GpuVideoDecoderMFT::CheckDecoderDxvaSupport() {
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
    LOG(ERROR) << "Failed to get DXVA attr, hr = "
               << std::hex << std::showbase << hr
               << "this might not be the right decoder.";
    return false;
  }
  return true;
}

bool GpuVideoDecoderMFT::SetDecoderMediaTypes() {
  return SetDecoderInputMediaType() &&
         SetDecoderOutputMediaType(MFVideoFormat_NV12);
}

bool GpuVideoDecoderMFT::SetDecoderInputMediaType() {
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

bool GpuVideoDecoderMFT::SetDecoderOutputMediaType(const GUID subtype) {
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
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to SetOutputType to |subtype| or obtain "
                   << "width/height/stride " << std::hex << hr;
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

bool GpuVideoDecoderMFT::SendMFTMessage(MFT_MESSAGE_TYPE msg) {
  HRESULT hr = decoder_->ProcessMessage(msg, NULL);
  return SUCCEEDED(hr);
}

// Prints out info about the input/output streams, gets the minimum buffer sizes
// for input and output samples.
// The MFT will not allocate buffer for neither input nor output, so we have
// to do it ourselves and make sure they're the correct size.
// Exception is when dxva is enabled, the decoder will allocate output.
bool GpuVideoDecoderMFT::GetStreamsInfoAndBufferReqs() {
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
  CHECK_EQ(static_cast<int>(input_stream_info_.dwFlags), 0x7);
  LOG(INFO) << "Min buffer size: " << input_stream_info_.cbSize;
  LOG(INFO) << "Max lookahead: " << input_stream_info_.cbMaxLookahead;
  LOG(INFO) << "Alignment: " << input_stream_info_.cbAlignment;
  if (input_stream_info_.cbAlignment > 0) {
    LOG(WARNING) << "Warning: Decoder requires input to be aligned";
  }

  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get output stream info";
    return false;
  }
  LOG(INFO) << "Output stream info: ";

  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  CHECK_EQ(static_cast<int>(output_stream_info_.dwFlags),  0x107);
  LOG(INFO) << "Min buffer size: " << output_stream_info_.cbSize;
  LOG(INFO) << "Alignment: " << output_stream_info_.cbAlignment;
  if (output_stream_info_.cbAlignment > 0) {
    LOG(WARNING) << "Warning: Decoder requires output to be aligned";
  }

  return true;
}

bool GpuVideoDecoderMFT::DoInitialize(
    const GpuVideoDecoderInitParam& param,
    GpuVideoDecoderInitDoneParam* done_param) {
  LOG(ERROR) << "GpuVideoDecoderMFT::DoInitialize";

  done_param->format_ =
    GpuVideoDecoderInitDoneParam::SurfaceFormat_YV12;
  done_param->surface_type_ =
    GpuVideoDecoderInitDoneParam::SurfaceTypeSystemMemory;
  done_param->input_buffer_handle_ = base::SharedMemory::NULLHandle();
  done_param->output_buffer_handle_ = base::SharedMemory::NULLHandle();

  do {
    done_param->success_ = false;

    if (!InitMediaFoundation())
      break;

    // TODO(jiesun): Check the assumption of input size < original size.
    done_param->input_buffer_size_ = param.width_ * param.height_ * 3 / 2;
    input_transfer_buffer_.reset(new base::SharedMemory);
    if (!input_transfer_buffer_->Create(std::wstring(), false, false,
                                        done_param->input_buffer_size_))
      break;
    if (!input_transfer_buffer_->Map(done_param->input_buffer_size_))
      break;

    // TODO(jiesun): Allocate this according to the surface format.
    // The format actually could change during streaming, we need to
    // notify GpuVideoDecoderHost side when this happened and renegotiate
    // the transfer buffer.
    done_param->output_buffer_size_ = param.width_ * param.height_ * 3 / 2;
    output_transfer_buffer_.reset(new base::SharedMemory);
    if (!output_transfer_buffer_->Create(std::wstring(), false, false,
                                         done_param->output_buffer_size_))
      break;
    if (!output_transfer_buffer_->Map(done_param->output_buffer_size_))
      break;

    if (!input_transfer_buffer_->ShareToProcess(
        renderer_handle_,
        &done_param->input_buffer_handle_))
      break;
    if (!output_transfer_buffer_->ShareToProcess(
        renderer_handle_,
        &done_param->output_buffer_handle_))
      break;

    done_param->success_ = true;
  } while (0);

  SendInitializeDone(*done_param);
  return true;
}

bool GpuVideoDecoderMFT::DoUninitialize() {
  LOG(ERROR) << "GpuVideoDecoderMFT::DoUninitialize";
  SendUninitializeDone();
  return true;
}

void GpuVideoDecoderMFT::DoEmptyThisBuffer(
    const GpuVideoDecoderInputBufferParam& buffer) {
  LOG(ERROR) << "GpuVideoDecoderMFT::EmptyThisBuffer";

  CHECK(input_transfer_buffer_->memory());
  ScopedComPtr<IMFSample> sample;
  if (buffer.size_) {
    uint8* data = static_cast<uint8*>(input_transfer_buffer_->memory());
    sample.Attach(CreateInputSample(data,
                                    buffer.size_,
                                    buffer.timestamp_*10,
                                    0LL,
                                    input_stream_info_.cbSize));
    CHECK(sample.get());
  } else {
    state_ = kEosFlush;
  }

  input_buffer_queue_.push_back(sample);
  SendEmptyBufferACK();

  while (pending_request_)
    if (!DoDecode()) break;
}

void GpuVideoDecoderMFT::DoFillThisBuffer(
    const GpuVideoDecoderOutputBufferParam& frame) {
  LOG(ERROR) << "GpuVideoDecoderMFT::FillThisBuffer";

  pending_request_++;
  while (pending_request_)
    if (!DoDecode()) break;
}

void GpuVideoDecoderMFT::DoFillThisBufferDoneACK() {
  output_transfer_buffer_busy_ = false;
  pending_request_--;
  while (pending_request_)
    if (!DoDecode()) break;
}

void GpuVideoDecoderMFT::DoFlush() {
  state_ = kFlushing;

  while (!input_buffer_queue_.empty())
    input_buffer_queue_.pop_front();
  pending_request_ = 0;
  // TODO(jiesun): this is wrong??
  output_transfer_buffer_busy_ = false;
  SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH);

  state_ = kNormal;
  SendFlushDone();
}

bool GpuVideoDecoderMFT::DoDecode() {
  if (state_ != kNormal && state_ != kEosFlush) return false;
  if (output_transfer_buffer_busy_) return false;

  MFT_OUTPUT_DATA_BUFFER output_data_buffer;
  memset(&output_data_buffer, 0, sizeof(output_data_buffer));
  output_data_buffer.dwStreamID = 0;

  ScopedComPtr<IMFSample> output_sample;
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

  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      hr = SetDecoderOutputMediaType(MFVideoFormat_NV12);
      CHECK(SUCCEEDED(hr));
      return true;
  }
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    if (input_buffer_queue_.empty()) {
      if (state_ == kEosFlush) {
        GpuVideoDecoderOutputBufferParam output_param;
        output_param.timestamp_ = 0;
        output_param.duration_ = 0;
        output_param.flags_ =
            GpuVideoDecoderOutputBufferParam::kFlagsEndOfStream;
        output_transfer_buffer_busy_ = true;
        SendFillBufferDone(output_param);
      }
      return false;
    }
    while (!input_buffer_queue_.empty()) {
      ScopedComPtr<IMFSample> input_sample = input_buffer_queue_.front();
      input_buffer_queue_.pop_front();

      if (input_sample.get()) {
        HRESULT hr = decoder_->ProcessInput(0, input_sample.get(), 0);
        if (hr == MF_E_NOTACCEPTING) return true;
        CHECK(SUCCEEDED(hr));
      } else {
        SendMFTMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM);
      }

      // If we already received the input EOS, we do not need to issue
      // more requests for new samples.
      if (state_ != kEosFlush)
        SendEmptyBufferDone();
    }
    return true;
  }

  CHECK(SUCCEEDED(hr));
  output_sample.Attach(output_data_buffer.pSample);
  CHECK(output_sample.get());

  int64 timestamp, duration;
  output_sample->GetSampleTime(&timestamp);
  output_sample->GetSampleDuration(&duration);

  // The duration and timestamps are in 100-ns units, so divide by 10
  // to convert to microseconds.
  timestamp /= 10;
  duration /= 10;

  // Sanity checks for checking if there is really something in the sample.
  DWORD buf_count;
  hr = output_sample->GetBufferCount(&buf_count);
  CHECK(SUCCEEDED(hr) && buf_count == 1);

  ScopedComPtr<IMFMediaBuffer> output_buffer;
  hr = output_sample->GetBufferByIndex(0, output_buffer.Receive());
  CHECK(SUCCEEDED(hr));

  ScopedComPtr<IDirect3DSurface9> surface;
  hr = MFGetService(output_buffer, MR_BUFFER_SERVICE,
                    IID_PPV_ARGS(surface.Receive()));
  CHECK(SUCCEEDED(hr));


  // NV12 to YV12
  D3DLOCKED_RECT d3dlocked_rect;
  RECT rect = {0, 0, init_param_.width_, init_param_.height_};
  hr = surface->LockRect(&d3dlocked_rect, &rect, 0);

  if (SUCCEEDED(hr)) {
    D3DSURFACE_DESC desc;
    hr = surface->GetDesc(&desc);
    CHECK(SUCCEEDED(hr));

    uint32 src_stride = d3dlocked_rect.Pitch;
    uint32 dst_stride = init_param_.width_;
    uint8* src_y = static_cast<uint8*>(d3dlocked_rect.pBits);
    uint8* src_uv = src_y + src_stride * desc.Height;
    uint8* dst_y = static_cast<uint8*>(output_transfer_buffer_->memory());
    uint8* dst_u = dst_y + dst_stride *  init_param_.height_;
    uint8* dst_v = dst_u + dst_stride *  init_param_.height_ / 4;

    for ( int y = 0 ; y < init_param_.height_; ++y ) {
      for ( int x = 0 ; x < init_param_.width_ ; ++x ) {
        dst_y[x] = src_y[x];
        if (!(y & 1)) {
          if (x & 1)
            dst_v[x>>1] = src_uv[x];
          else
            dst_u[x>>1] = src_uv[x];
        }
      }
      dst_y += dst_stride;
      src_y += src_stride;
      if (!(y & 1)) {
        src_uv += src_stride;
        dst_v += dst_stride >> 1;
        dst_u += dst_stride >> 1;
      }
    }
    hr = surface->UnlockRect();
    CHECK(SUCCEEDED(hr));
  }

  GpuVideoDecoderOutputBufferParam output_param;
  output_param.timestamp_ = timestamp;
  output_param.duration_ = duration;
  output_param.flags_ = 0;
  output_transfer_buffer_busy_ = true;
  SendFillBufferDone(output_param);
  return true;
}

#endif

