// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/mft_h264_decode_engine.h"

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
#include "media/base/limits.h"
#include "media/base/pipeline.h"
#include "media/video/video_decode_context.h"

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
  VLOG(1) << __FUNCTION__ << " wrote " << size << " bytes into input sample";
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

MftH264DecodeEngine::MftH264DecodeEngine(bool use_dxva)
    : use_dxva_(use_dxva),
      state_(kUninitialized),
      event_handler_(NULL),
      context_(NULL) {
  memset(&input_stream_info_, 0, sizeof(input_stream_info_));
  memset(&output_stream_info_, 0, sizeof(output_stream_info_));
  memset(&config_, 0, sizeof(config_));
  memset(&info_, 0, sizeof(info_));
}

MftH264DecodeEngine::~MftH264DecodeEngine() {
}

void MftH264DecodeEngine::Initialize(
    MessageLoop* message_loop,
    VideoDecodeEngine::EventHandler* event_handler,
    VideoDecodeContext* context,
    const VideoCodecConfig& config) {
  DCHECK(!use_dxva_ || context);
  if (state_ != kUninitialized) {
    LOG(ERROR) << "Initialize: invalid state";
    return;
  }
  if (!message_loop || !event_handler) {
    LOG(ERROR) << "MftH264DecodeEngine::Initialize: parameters cannot be NULL";
    return;
  }
  context_ = context;
  config_ = config;
  event_handler_ = event_handler;
  info_.provides_buffers = true;

  if (use_dxva_) {
    info_.stream_info.surface_format = VideoFrame::NV12;
    // TODO(hclam): Need to correct this since this is not really GL texture.
    // We should just remove surface_type from stream_info.
    info_.stream_info.surface_type = VideoFrame::TYPE_GL_TEXTURE;
  } else {
    info_.stream_info.surface_format = VideoFrame::YV12;
    info_.stream_info.surface_type = VideoFrame::TYPE_SYSTEM_MEMORY;
  }

  // codec_info.stream_info_.surface_width_/height_ are initialized
  // in InitInternal().
  info_.success = InitInternal();
  if (info_.success) {
    state_ = kNormal;
    AllocFramesFromContext();
  } else {
    LOG(ERROR) << "MftH264DecodeEngine::Initialize failed";
    event_handler_->OnInitializeComplete(info_);
  }
}

void MftH264DecodeEngine::Uninitialize() {
  if (state_ == kUninitialized) {
    LOG(ERROR) << "Uninitialize: invalid state";
    return;
  }

  // TODO(hclam): Call ShutdownComLibraries only after MFT is released.
  decode_engine_.Release();
  ShutdownComLibraries();
  state_ = kUninitialized;
  event_handler_->OnUninitializeComplete();
}

void MftH264DecodeEngine::Flush() {
  if (state_ != kNormal) {
    LOG(ERROR) << "Flush: invalid state";
    return;
  }
  state_ = kFlushing;
  if (!SendMFTMessage(MFT_MESSAGE_COMMAND_FLUSH)) {
    LOG(WARNING) << "MftH264DecodeEngine::Flush failed to send message";
  }
  state_ = kNormal;
  event_handler_->OnFlushComplete();
}

void MftH264DecodeEngine::Seek() {
  if (state_ != kNormal) {
    LOG(ERROR) << "Seek: invalid state";
    return;
  }

  // TODO(hclam): Seriously the logic in VideoRendererBase is flawed that we
  // have to perform the following hack to get playback going.
  PipelineStatistics statistics;
  for (size_t i = 0; i < Limits::kMaxVideoFrames; ++i) {
    event_handler_->ConsumeVideoFrame(output_frames_[0], statistics);
  }

  // Seek not implemented.
  event_handler_->OnSeekComplete();
}

void MftH264DecodeEngine::ConsumeVideoSample(scoped_refptr<Buffer> buffer) {
  if (state_ == kUninitialized) {
    LOG(ERROR) << "ConsumeVideoSample: invalid state";
  }
  ScopedComPtr<IMFSample> sample;
  PipelineStatistics statistics;
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
      if (FAILED(decode_engine_->ProcessInput(0, sample.get(), 0))) {
        event_handler_->OnError();
      }
    }

    statistics.video_bytes_decoded = buffer->GetDataSize();
  } else {
    if (state_ != MftH264DecodeEngine::kEosDrain) {
      // End of stream, send drain messages.
      if (!SendMFTMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM) ||
          !SendMFTMessage(MFT_MESSAGE_COMMAND_DRAIN)) {
        LOG(ERROR) << "Failed to send EOS / drain messages to MFT";
        event_handler_->OnError();
      } else {
        state_ = MftH264DecodeEngine::kEosDrain;
      }
    }
  }
  DoDecode(statistics);
}

void MftH264DecodeEngine::ProduceVideoFrame(scoped_refptr<VideoFrame> frame) {
  if (state_ == kUninitialized) {
    LOG(ERROR) << "ProduceVideoFrame: invalid state";
    return;
  }
  event_handler_->ProduceVideoSample(NULL);
}

// private methods

// static
bool MftH264DecodeEngine::StartupComLibraries() {
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
void MftH264DecodeEngine::ShutdownComLibraries() {
  HRESULT hr;
  hr = MFShutdown();
  if (FAILED(hr)) {
    LOG(WARNING) << "Warning: MF failed to shutdown";
  }
  CoUninitialize();
}

bool MftH264DecodeEngine::EnableDxva() {
  IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(
      context_->GetDevice());
  ScopedComPtr<IDirect3DDeviceManager9> device_manager;
  UINT dev_manager_reset_token = 0;
  HRESULT hr = DXVA2CreateDirect3DDeviceManager9(&dev_manager_reset_token,
                                                 device_manager.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Couldn't create D3D Device manager";
    return false;
  }

  hr = device_manager->ResetDevice(device, dev_manager_reset_token);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to reset device";
    return false;
  }

  hr = decode_engine_->ProcessMessage(
      MFT_MESSAGE_SET_D3D_MANAGER,
      reinterpret_cast<ULONG_PTR>(device_manager.get()));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set D3D9 device manager to decoder "
               << std::hex << hr;
    return false;
  }

  return true;
}

bool MftH264DecodeEngine::InitInternal() {
  if (!StartupComLibraries())
    return false;
  if (!InitDecodeEngine())
    return false;
  if (!GetStreamsInfoAndBufferReqs())
    return false;
  return SendMFTMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING);
}

bool MftH264DecodeEngine::InitDecodeEngine() {
  // TODO(jiesun): use MFEnum to get decoder CLSID.
  HRESULT hr = CoCreateInstance(__uuidof(CMSH264DecoderMFT),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMFTransform),
                                reinterpret_cast<void**>(
                                    decode_engine_.Receive()));
  if (FAILED(hr) || !decode_engine_.get()) {
    LOG(ERROR) << "CoCreateInstance failed " << std::hex << std::showbase << hr;
    return false;
  }
  if (!CheckDecodeEngineDxvaSupport())
    return false;
  if (use_dxva_ && !EnableDxva())
    return false;
  return SetDecodeEngineMediaTypes();
}

void MftH264DecodeEngine::AllocFramesFromContext() {
  if (!use_dxva_)
    return;

  // TODO(imcheng): Pass in an actual task. (From EventHandler?)
  context_->ReleaseAllVideoFrames();
  output_frames_.clear();
  context_->AllocateVideoFrames(
      1, info_.stream_info.surface_width, info_.stream_info.surface_height,
      VideoFrame::RGBA, &output_frames_,
      NewRunnableMethod(this, &MftH264DecodeEngine::OnAllocFramesDone));
}

void MftH264DecodeEngine::OnAllocFramesDone() {
  event_handler_->OnInitializeComplete(info_);
}

bool MftH264DecodeEngine::CheckDecodeEngineDxvaSupport() {
  ScopedComPtr<IMFAttributes> attributes;
  HRESULT hr = decode_engine_->GetAttributes(attributes.Receive());
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

bool MftH264DecodeEngine::SetDecodeEngineMediaTypes() {
  if (!SetDecodeEngineInputMediaType())
    return false;
  return SetDecodeEngineOutputMediaType(
      ConvertVideoFrameFormatToGuid(info_.stream_info.surface_format));
}

bool MftH264DecodeEngine::SetDecodeEngineInputMediaType() {
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

  hr = decode_engine_->SetInputType(0, media_type.get(), 0);  // No flags
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set decoder's input type";
    return false;
  }

  return true;
}

bool MftH264DecodeEngine::SetDecodeEngineOutputMediaType(const GUID subtype) {
  DWORD i = 0;
  IMFMediaType* out_media_type;
  bool found = false;
  while (SUCCEEDED(decode_engine_->GetOutputAvailableType(0, i,
                                                          &out_media_type))) {
    GUID out_subtype;
    HRESULT hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to GetGUID() on GetOutputAvailableType() " << i;
      out_media_type->Release();
      continue;
    }
    if (out_subtype == subtype) {
      hr = decode_engine_->SetOutputType(0, out_media_type, 0);  // No flags
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

bool MftH264DecodeEngine::SendMFTMessage(MFT_MESSAGE_TYPE msg) {
  HRESULT hr = decode_engine_->ProcessMessage(msg, NULL);
  return SUCCEEDED(hr);
}

// Prints out info about the input/output streams, gets the minimum buffer sizes
// for input and output samples.
// The MFT will not allocate buffer for neither input nor output, so we have
// to do it ourselves and make sure they're the correct size.
// Exception is when dxva is enabled, the decoder will allocate output.
bool MftH264DecodeEngine::GetStreamsInfoAndBufferReqs() {
  HRESULT hr = decode_engine_->GetInputStreamInfo(0, &input_stream_info_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get input stream info";
    return false;
  }
  VLOG(1) << "Input stream info:"
          << "\nMax latency: " << input_stream_info_.hnsMaxLatency
          << "\nFlags: " << std::hex << std::showbase
          << input_stream_info_.dwFlags
          << "\nMin buffer size: " << input_stream_info_.cbSize
          << "\nMax lookahead: " << input_stream_info_.cbMaxLookahead
          << "\nAlignment: " << input_stream_info_.cbAlignment;
  // There should be three flags, one for requiring a whole frame be in a
  // single sample, one for requiring there be one buffer only in a single
  // sample, and one that specifies a fixed sample size. (as in cbSize)
  CHECK_EQ(input_stream_info_.dwFlags, 0x7u);

  hr = decode_engine_->GetOutputStreamInfo(0, &output_stream_info_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get output stream info";
    return false;
  }
  VLOG(1) << "Output stream info:"
          << "\nFlags: " << std::hex << std::showbase
          << output_stream_info_.dwFlags
          << "\nMin buffer size: " << output_stream_info_.cbSize
          << "\nAlignment: " << output_stream_info_.cbAlignment;
  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  CHECK_EQ(output_stream_info_.dwFlags, use_dxva_ ? 0x107u : 0x7u);

  return true;
}

bool MftH264DecodeEngine::DoDecode(const PipelineStatistics& statistics) {
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
  HRESULT hr = decode_engine_->ProcessOutput(0,  // No flags
                                             1,  // # of out streams to pull
                                             &output_data_buffer,
                                             &status);

  IMFCollection* events = output_data_buffer.pEvents;
  if (events != NULL) {
    VLOG(1) << "Got events from ProcessOuput, but discarding";
    events->Release();
  }

  if (FAILED(hr)) {
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      hr = SetDecodeEngineOutputMediaType(
          ConvertVideoFrameFormatToGuid(info_.stream_info.surface_format));
      if (SUCCEEDED(hr)) {
        // TODO(hclam): Need to fix this case. This happens when we have a
        // format change. We have to resume decoding only after we have
        // allocated a new set of video frames.
        // AllocFramesFromContext();
        // event_handler_->OnFormatChange(info_.stream_info);
        event_handler_->ProduceVideoSample(NULL);
        return true;
      }
      event_handler_->OnError();
      return false;
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      if (state_ == kEosDrain) {
        // No more output from the decoder. Notify EOS and stop playback.
        scoped_refptr<VideoFrame> frame;
        VideoFrame::CreateEmptyFrame(&frame);
        event_handler_->ConsumeVideoFrame(frame, statistics);
        state_ = MftH264DecodeEngine::kStopped;
        return false;
      }
      event_handler_->ProduceVideoSample(NULL);
      return true;
    }
    LOG(ERROR) << "Unhandled error in DoDecode()";
    state_ = MftH264DecodeEngine::kStopped;
    event_handler_->OnError();
    return false;
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
    ScopedComPtr<IDirect3DSurface9, &IID_IDirect3DSurface9> surface;
    hr = MFGetService(output_buffer, MR_BUFFER_SERVICE,
                      IID_PPV_ARGS(surface.Receive()));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get surface from buffer";
      return true;
    }
    // Since we only allocated 1 frame from context.
    // TODO(imcheng): Detect error.
    output_frames_[0]->SetTimestamp(TimeDelta::FromMicroseconds(timestamp));
    output_frames_[0]->SetDuration(TimeDelta::FromMicroseconds(duration));
    context_->ConvertToVideoFrame(
        surface.get(), output_frames_[0],
        NewRunnableMethod(this, &MftH264DecodeEngine::OnUploadVideoFrameDone,
                          surface, output_frames_[0], statistics));
    return true;
  }
  // TODO(hclam): Remove this branch.
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
  hr = output_buffer->Lock(&src_y, &max_length, &current_length);
  if (FAILED(hr))
    return true;
  uint8* dst_y = static_cast<uint8*>(frame->data(VideoFrame::kYPlane));

  memcpy(dst_y, src_y, current_length);
  CHECK(SUCCEEDED(output_buffer->Unlock()));
  event_handler_->ConsumeVideoFrame(frame, statistics);
  return true;
}

void MftH264DecodeEngine::OnUploadVideoFrameDone(
    ScopedComPtr<IDirect3DSurface9, &IID_IDirect3DSurface9> surface,
    scoped_refptr<media::VideoFrame> frame,
    PipelineStatistics statistics) {
  // After this method is exited the reference to surface is released.
  event_handler_->ConsumeVideoFrame(frame, statistics);
}

}  // namespace media

DISABLE_RUNNABLE_METHOD_REFCOUNT(media::MftH264DecodeEngine);
