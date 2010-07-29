// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/mf/h264mft.h"

#include <algorithm>
#include <string>

#include <d3d9.h>
#include <evr.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <shlwapi.h>
#include <wmcodecdsp.h>

#include "base/logging.h"
#include "base/scoped_comptr_win.h"
#include "media/base/video_frame.h"
#include "media/mf/file_reader_util.h"

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mfplat.lib")

namespace media {

// Returns Media Foundation's H.264 decoder as an MFT, or NULL if not found
// (e.g. Not using Windows 7)
static IMFTransform* GetH264Decoder() {
  // Use __uuidof() to avoid linking to a library just for the CLSID.
  IMFTransform* dec;
  HRESULT hr = CoCreateInstance(__uuidof(CMSH264DecoderMFT), NULL,
                                 CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dec));
  if (FAILED(hr)) {
    LOG(ERROR) << "CoCreateInstance failed " << std::hex << std::showbase << hr;
    return NULL;
  }
  return dec;
}

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

// Creates a Media Foundation sample with one buffer of length |buffer_length|.
static IMFSample* CreateEmptySampleWithBuffer(int buffer_length) {
  CHECK_GT(buffer_length, 0);
  ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySample());
  if (sample.get() == NULL)
    return NULL;
  ScopedComPtr<IMFMediaBuffer> buffer;
  HRESULT hr;
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
  return sample.Detach();
}

// Creates a Media Foundation sample with one buffer containing a copy of the
// given Annex B stream data.
// If duration and sample_time are not known, provide 0.
// min_size specifies the minimum size of the buffer (might be required by
// the decoder for input). The times here should be given in 100ns units.
static IMFSample* CreateInputSample(uint8* stream, int size,
                                    int64 timestamp, int64 duration,
                                    int min_size) {
  CHECK(stream != NULL);
  CHECK_GT(size, 0);
  ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateEmptySampleWithBuffer(std::max(min_size, size)));
  if (sample.get() == NULL) {
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
  CHECK_EQ(static_cast<int>(current_length), 0);
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

// Public methods

H264Mft::H264Mft(bool use_dxva)
    : decoder_(NULL),
      initialized_(false),
      use_dxva_(use_dxva),
      drain_message_sent_(false),
      in_buffer_size_(0),
      out_buffer_size_(0),
      frames_read_(0),
      frames_decoded_(0),
      width_(0),
      height_(0),
      stride_(0) {
}

H264Mft::~H264Mft() {
}

bool H264Mft::Init(IDirect3DDeviceManager9* dev_manager,
                   int frame_rate_num, int frame_rate_denom,
                   int width, int height,
                   int aspect_num, int aspect_denom) {
  if (initialized_)
    return true;
  if (!InitDecoder(dev_manager, frame_rate_num, frame_rate_denom,
                   width, height, aspect_num, aspect_denom))
    return false;
  if (!GetStreamsInfoAndBufferReqs())
    return false;
  if (!SendStartMessage())
    return false;
  initialized_ = true;
  return true;
}

bool H264Mft::SendInput(uint8* data, int size, int64 timestamp,
                        int64 duration) {
  CHECK(initialized_);
  CHECK(data != NULL);
  CHECK_GT(size, 0);
  if (drain_message_sent_) {
    LOG(ERROR) << "Drain message was already sent, but trying to send more "
                  "input to decoder";
    return false;
  }
  ScopedComPtr<IMFSample> sample;
  sample.Attach(CreateInputSample(data, size, timestamp, duration,
                                  in_buffer_size_));
  if (sample.get() == NULL) {
    LOG(ERROR) << "Failed to convert input stream to sample";
    return false;
  }
  HRESULT hr = decoder_->ProcessInput(0, sample.get(), 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to ProcessInput, hr = " << std::hex << hr;
    return false;
  }
  frames_read_++;
  return true;
}

static const char* const ProcessOutputStatusToCString(HRESULT hr) {
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
    return "media stream change occurred, need to set output type";
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
    return "decoder needs more samples";
  else
    return "unhandled error from ProcessOutput";
}

H264Mft::DecoderOutputState H264Mft::GetOutput(
    scoped_refptr<VideoFrame>* decoded_frame) {
  CHECK(initialized_);
  CHECK(decoded_frame != NULL);

  ScopedComPtr<IMFSample> output_sample;
  if (!use_dxva_) {
    // If DXVA is enabled, the decoder will allocate the sample for us.
    output_sample.Attach(CreateEmptySampleWithBuffer(out_buffer_size_));
    if (output_sample.get() == NULL) {
      LOG(ERROR) << "GetSample: failed to create empty output sample";
      return kNoMemory;
    }
  }
  MFT_OUTPUT_DATA_BUFFER output_data_buffer;
  output_data_buffer.dwStreamID = 0;
  output_data_buffer.pSample = output_sample;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = NULL;
  DWORD status;
  HRESULT hr;
  hr = decoder_->ProcessOutput(0,            // No flags
                               1,            // # of out streams to pull from
                               &output_data_buffer,
                               &status);

  // TODO(imcheng): Handle the events, if any. (No event is returned most of
  // the time.)
  IMFCollection* events = output_data_buffer.pEvents;
  if (events != NULL) {
    LOG(INFO) << "Got events from ProcessOuput, but discarding";
    events->Release();
  }
  if (FAILED(hr)) {
    LOG(INFO) << "ProcessOutput failed with status " << std::hex << hr
              << ", meaning..." << ProcessOutputStatusToCString(hr);
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      if (!SetDecoderOutputMediaType(MFVideoFormat_NV12)) {
        LOG(ERROR) << "Failed to reset output type";
        return kResetOutputStreamFailed;
       } else {
        LOG(INFO) << "Reset output type done";
        return kResetOutputStreamOk;
      }
    } else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      // At this point we have either read everything from file or we can
      // still feed the decoder input. If we have read everything then we
      // should've sent a drain message to the MFT. If the drain message is
      // sent but it doesn't give out anymore output then we know the decoder
      // has processed everything.
      if (drain_message_sent_) {
        LOG(INFO) << "Drain message was already sent + no output => done";
        return kNoMoreOutput;
      } else {
        return kNeedMoreInput;
      }
    } else {
      return kUnspecifiedError;
    }
  } else {
    // A decoded sample was successfully obtained.
    LOG(INFO) << "Got a decoded sample from decoder";
    if (use_dxva_) {
      // If dxva is enabled, we did not provide a sample to ProcessOutput,
      // i.e. output_sample is NULL.
      output_sample.Attach(output_data_buffer.pSample);
      if (output_sample.get() == NULL) {
        LOG(ERROR) << "Output sample using DXVA is NULL - ProcessOutput did "
                   << "not provide it!";
        return kOutputSampleError;
      }
    }
    int64 timestamp, duration;
    hr = output_sample->GetSampleTime(&timestamp);
    hr = output_sample->GetSampleDuration(&duration);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get sample duration or timestamp "
                 << std::hex << hr;
      return kOutputSampleError;
    }

    // The duration and timestamps are in 100-ns units, so divide by 10
    // to convert to microseconds.
    timestamp /= 10;
    duration /= 10;

    // Sanity checks for checking if there is really something in the sample.
    DWORD buf_count;
    hr = output_sample->GetBufferCount(&buf_count);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get buff count, hr = " << std::hex << hr;
      return kOutputSampleError;
    }
    if (buf_count == 0) {
      LOG(ERROR) << "buf_count is 0, dropping sample";
      return kOutputSampleError;
    }
    ScopedComPtr<IMFMediaBuffer> out_buffer;
    hr = output_sample->GetBufferByIndex(0, out_buffer.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get decoded output buffer";
      return kOutputSampleError;
    }

    // To obtain the data, the caller should call the Lock() method instead
    // of using the data field.
    // In NV12, there are only 2 planes - the Y plane, and the interleaved UV
    // plane. Both have the same strides.
    uint8* null_data[2] = { NULL, NULL };
    int32 strides[2] = { stride_, stride_ };
    VideoFrame::CreateFrameExternal(
        use_dxva_ ? VideoFrame::TYPE_DIRECT3DSURFACE :
                    VideoFrame::TYPE_MFBUFFER,
        VideoFrame::NV12,
        width_,
        height_,
        2,
        null_data,
        strides,
        base::TimeDelta::FromMicroseconds(timestamp),
        base::TimeDelta::FromMicroseconds(duration),
        out_buffer.Detach(),
        decoded_frame);
    CHECK(decoded_frame->get() != NULL);
    frames_decoded_++;
    return kOutputOk;
  }
}

bool H264Mft::SendDrainMessage() {
  CHECK(initialized_);
  if (drain_message_sent_) {
    LOG(ERROR) << "Drain message was already sent before!";
    return false;
  }

  // Send the drain message with no parameters.
  HRESULT hr = decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to send the drain message to decoder";
    return false;
  }
  drain_message_sent_ = true;
  return true;
}

// Private methods

bool H264Mft::InitDecoder(IDirect3DDeviceManager9* dev_manager,
                          int frame_rate_num, int frame_rate_denom,
                          int width, int height,
                          int aspect_num, int aspect_denom) {
  decoder_.Attach(GetH264Decoder());
  if (!decoder_.get())
    return false;
  if (!CheckDecoderProperties())
    return false;
  if (use_dxva_) {
    if (!CheckDecoderDxvaSupport())
    return false;
  if (!SetDecoderD3d9Manager(dev_manager))
    return false;
  }
  if (!SetDecoderMediaTypes(frame_rate_num, frame_rate_denom,
                            width, height,
                            aspect_num, aspect_denom)) {
    return false;
  }
  return true;
}

bool H264Mft::CheckDecoderProperties() {
  DCHECK(decoder_.get());
  DWORD in_stream_count;
  DWORD out_stream_count;
  HRESULT hr;
  hr = decoder_->GetStreamCount(&in_stream_count, &out_stream_count);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get stream count";
    return false;
  } else {
    LOG(INFO) << "Input stream count: " << in_stream_count << ", "
              << "Output stream count: " << out_stream_count;
    bool mismatch = false;
    if (in_stream_count != 1) {
      LOG(ERROR) << "Input stream count mismatch!";
      mismatch = true;
    }
    if (out_stream_count != 1) {
      LOG(ERROR) << "Output stream count mismatch!";
      mismatch = true;
    }
    return !mismatch;
  }
}

bool H264Mft::CheckDecoderDxvaSupport() {
  HRESULT hr;
  ScopedComPtr<IMFAttributes> attributes;
  hr = decoder_->GetAttributes(attributes.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unlock: Failed to get attributes, hr = "
               << std::hex << std::showbase << hr;
    return false;
  }
  UINT32 dxva;
  hr = attributes->GetUINT32(MF_SA_D3D_AWARE, &dxva);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get DXVA attr, hr = "
               << std::hex << std::showbase << hr
               << "this might not be the right decoder.";
    return false;
  }
  LOG(INFO) << "Support dxva? " << dxva;
  if (!dxva) {
    LOG(ERROR) << "Decoder does not support DXVA - this might not be the "
               << "right decoder.";
    return false;
  }
  return true;
}

bool H264Mft::SetDecoderD3d9Manager(IDirect3DDeviceManager9* dev_manager) {
  DCHECK(use_dxva_) << "SetDecoderD3d9Manager should only be called if DXVA is "
                    << "enabled";
  CHECK(dev_manager != NULL);
  HRESULT hr;
  hr = decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
      reinterpret_cast<ULONG_PTR>(dev_manager));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set D3D9 device to decoder";
    return false;
  }
  return true;
}

bool H264Mft::SetDecoderMediaTypes(int frame_rate_num, int frame_rate_denom,
                                   int width, int height,
                                   int aspect_num, int aspect_denom) {
  DCHECK(decoder_.get());
  if (!SetDecoderInputMediaType(frame_rate_num, frame_rate_denom,
                                width, height,
                                aspect_num, aspect_denom))
    return false;
  if (!SetDecoderOutputMediaType(MFVideoFormat_NV12)) {
    return false;
  }
  return true;
}

bool H264Mft::SetDecoderInputMediaType(int frame_rate_num, int frame_rate_denom,
                                       int width, int height,
                                       int aspect_num, int aspect_denom) {
  ScopedComPtr<IMFMediaType> media_type;
  HRESULT hr;
  hr = MFCreateMediaType(media_type.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create empty media type object";
    return NULL;
  }
  hr = media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    LOG(ERROR) << "SetGUID for major type failed";
    return NULL;
  }
  hr = media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  if (FAILED(hr)) {
    LOG(ERROR) << "SetGUID for subtype failed";
    return NULL;
  }

  // Provide additional info to the decoder to avoid a format change during
  // streaming.
  if (frame_rate_num == 0 || frame_rate_denom == 0) {
    hr = MFSetAttributeRatio(media_type.get(), MF_MT_FRAME_RATE,
                             frame_rate_num, frame_rate_denom);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to set frame rate";
      return NULL;
    }
  }
  if (width == 0 || height == 0) {
    hr = MFSetAttributeSize(media_type.get(), MF_MT_FRAME_SIZE, width, height);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to set frame size";
      return NULL;
    }
  }

  // TODO(imcheng): Not sure about this, but this is the recommended value by
  // MSDN.
  hr = media_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_MixedInterlaceOrProgressive);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set interlace mode";
    return NULL;
  }
  if (aspect_num == 0 || aspect_denom == 0) {
    hr = MFSetAttributeRatio(media_type.get(), MF_MT_PIXEL_ASPECT_RATIO,
                             aspect_num, aspect_denom);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get aspect ratio";
      return NULL;
    }
  }
  hr = decoder_->SetInputType(0, media_type.get(), 0);  // No flags
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set decoder's input type";
    return false;
  }
  return true;
}

bool H264Mft::SetDecoderOutputMediaType(const GUID subtype) {
  DWORD i = 0;
  IMFMediaType* out_media_type;
  bool found = false;
  while (SUCCEEDED(decoder_->GetOutputAvailableType(0, i, &out_media_type))) {
    GUID out_subtype;
    HRESULT hr;
    hr = out_media_type->GetGUID(MF_MT_SUBTYPE, &out_subtype);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to GetGUID() on GetOutputAvailableType() " << i;
      out_media_type->Release();
      continue;
    }
    if (out_subtype == subtype) {
      LOG(INFO) << "|subtype| is at index "
                << i << " in GetOutputAvailableType()";
      hr = decoder_->SetOutputType(0, out_media_type, 0);  // No flags
      hr = MFGetAttributeSize(out_media_type, MF_MT_FRAME_SIZE,
                              reinterpret_cast<UINT32*>(&width_),
                              reinterpret_cast<UINT32*>(&height_));
      hr = MFGetStrideForBitmapInfoHeader(
          MFVideoFormat_NV12.Data1,
          width_,
          reinterpret_cast<LONG*>(&stride_));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to SetOutputType to |subtype| or obtain "
                   << "width/height/stride " << std::hex << hr;
      } else {
        found = true;
        out_media_type->Release();
        break;
      }
    }
    i++;
    out_media_type->Release();
  }
  if (!found) {
    LOG(ERROR) << "NV12 was not found in GetOutputAvailableType()";
    return false;
  }
  return true;
}

bool H264Mft::SendStartMessage() {
  HRESULT hr;
  hr = decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "Process start message failed, hr = "
               << std::hex << std::showbase << hr;
    return false;
  } else {
    LOG(INFO) << "Sent a message to decoder to indicate start of stream";
    return true;
  }
}

// Prints out info about the input/output streams, gets the minimum buffer sizes
// for input and output samples.
// The MFT will not allocate buffer for neither input nor output, so we have
// to do it ourselves and make sure they're the correct size.
// Exception is when dxva is enabled, the decoder will allocate output.
bool H264Mft::GetStreamsInfoAndBufferReqs() {
  DCHECK(decoder_.get());
  HRESULT hr;
  MFT_INPUT_STREAM_INFO input_stream_info;
  hr = decoder_->GetInputStreamInfo(0, &input_stream_info);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get input stream info";
    return false;
  }
  LOG(INFO) << "Input stream info: ";
  LOG(INFO) << "Max latency: " << input_stream_info.hnsMaxLatency;

  // There should be three flags, one for requiring a whole frame be in a
  // single sample, one for requiring there be one buffer only in a single
  // sample, and one that specifies a fixed sample size. (as in cbSize)
  LOG(INFO) << "Flags: "
            << std::hex << std::showbase << input_stream_info.dwFlags;
  CHECK_EQ(static_cast<int>(input_stream_info.dwFlags), 0x7);
  LOG(INFO) << "Min buffer size: " << input_stream_info.cbSize;
  LOG(INFO) << "Max lookahead: " << input_stream_info.cbMaxLookahead;
  LOG(INFO) << "Alignment: " << input_stream_info.cbAlignment;
  if (input_stream_info.cbAlignment > 0) {
    LOG(WARNING) << "Warning: Decoder requires input to be aligned";
  }
  in_buffer_size_ = input_stream_info.cbSize;

  MFT_OUTPUT_STREAM_INFO output_stream_info;
  hr = decoder_->GetOutputStreamInfo(0, &output_stream_info);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get output stream info";
    return false;
  }
  LOG(INFO) << "Output stream info: ";

  // The flags here should be the same and mean the same thing, except when
  // DXVA is enabled, there is an extra 0x100 flag meaning decoder will
  // allocate its own sample.
  LOG(INFO) << "Flags: "
            << std::hex << std::showbase << output_stream_info.dwFlags;
  CHECK_EQ(static_cast<int>(output_stream_info.dwFlags),
           use_dxva_ ? 0x107 : 0x7);
  LOG(INFO) << "Min buffer size: " << output_stream_info.cbSize;
  LOG(INFO) << "Alignment: " << output_stream_info.cbAlignment;
  if (output_stream_info.cbAlignment > 0) {
    LOG(WARNING) << "Warning: Decoder requires output to be aligned";
  }
  out_buffer_size_ = output_stream_info.cbSize;

  return true;
}

}  // namespace media
