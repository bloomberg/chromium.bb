// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifdef WINVER
#undef WINVER
#define WINVER 0x0601  // Windows 7
#endif

#include <d3d9.h>
#include <dxva2api.h>
#include <evr.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>  // depends on evr.h
#include <windows.h>

#include "base/logging.h"
#include "base/scoped_comptr_win.h"
#include "media/tools/mfdecoder/mfdecoder.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "evr.lib")
#pragma comment(lib, "mfreadwrite.lib")

namespace media {

MFDecoder::MFDecoder(bool use_dxva2)
    : width_(0),
      height_(0),
      use_dxva2_(use_dxva2),
      initialized_(false),
      com_lib_initialized_(false),
      mf_lib_initialized_(false),
      reader_(NULL),
      video_stream_index_(-1),
      mfbuffer_stride_(0),
      end_of_stream_(false) {
}

MFDecoder::~MFDecoder() {
  if (reader_)
    reader_->Release();
  if (com_lib_initialized_)
    CoUninitialize();
  if (mf_lib_initialized_)
    MFShutdown();
}

bool MFDecoder::Init(const wchar_t* source_url,
                     IDirect3DDeviceManager9* dev_manager) {
  if (initialized_)
    return true;
  if (source_url == NULL) {
    LOG(ERROR) << "Init: source_url cannot be NULL";
    return false;
  }
  if (use_dxva2_ && dev_manager == NULL) {
    LOG(ERROR) << "Init failed: DXVA2 specified, but no manager provided";
    return false;
  } else if (!use_dxva2_ && dev_manager != NULL) {
    LOG(WARNING) << "Init: Warning: DXVA2 not specified but manager is "
                 << "provided -- the manager will be ignored";
    dev_manager = NULL;
  }
  if (!InitLibraries())
    return false;
  if (!InitSourceReader(source_url, dev_manager))
    return false;

  // By now, |reader_| should be initialized.
  if (!SelectVideoStreamOnly())
    return false;

  // |video_stream_index_| should be pointing to the video stream now.
  if (!InitVideoInfo(dev_manager))
    return false;

  initialized_ = true;
  return true;
}

IMFSample* MFDecoder::ReadVideoSample() {
  CHECK(reader_ != NULL);
  CHECK_GE(video_stream_index_, 0);
  ScopedComPtr<IMFSample> video_sample;
  DWORD actual_stream_index;
  DWORD output_flags;

  // TODO(imcheng): Get timestamp back instead by passing in a timestamp pointer
  // instead of NULL.
  // TODO(imcheng): Read samples asynchronously and use callbacks.
  HRESULT hr = reader_->ReadSample(video_stream_index_,
                                   0,                     // No flags.
                                   &actual_stream_index,
                                   &output_flags,
                                   NULL,
                                   video_sample.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to read video sample";
    return NULL;
  } else {
    if (output_flags & MF_SOURCE_READERF_ERROR) {
      LOG(ERROR) << "output_flag error while reading video sample";
      return NULL;
    }
    if (output_flags & MF_SOURCE_READERF_ENDOFSTREAM) {
      LOG(INFO) << "Video sample reading has reached the end of stream";
      end_of_stream_ = true;
      return NULL;
    }
    if (static_cast<int>(actual_stream_index) != video_stream_index_) {
      LOG(ERROR) << "Received sample from stream " << actual_stream_index
                 << " instead of intended video stream " << video_stream_index_;
      return NULL;
    }
    if (video_sample.get() == NULL)
      LOG(WARNING) << "Video sample is NULL and not at end of stream!";
    return video_sample.Detach();
  }
}

// Private methods

bool MFDecoder::InitLibraries() {
  // TODO(imcheng): Move initialization to a singleton.
  HRESULT hr;
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    LOG(ERROR) << "CoInitializeEx failed during InitLibraries()";
    return false;
  }
  com_lib_initialized_ = true;

  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "MFStartup failed during InitLibraries()";
    CoUninitialize();
    com_lib_initialized_ = false;
    return false;
  }
  mf_lib_initialized_ = true;

  return true;
}

bool MFDecoder::InitSourceReader(const wchar_t* source_url,
                                 IDirect3DDeviceManager9* dev_manager) {
  CHECK(source_url != NULL);
  ScopedComPtr<IMFAttributes> reader_attributes;
  if (use_dxva2_) {
    reader_attributes.Attach(GetDXVA2AttributesForSourceReader(dev_manager));
    if (reader_attributes == NULL) {
      LOG(ERROR) << "Failed to create DXVA2 attributes for source reader";
      return false;
    }
  }
  HRESULT hr = MFCreateSourceReaderFromURL(source_url, reader_attributes.get(),
                                           &reader_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create source reader";
    return false;
  }
  LOG(INFO) << "Source reader created";
  return true;
}

IMFAttributes* MFDecoder::GetDXVA2AttributesForSourceReader(
    IDirect3DDeviceManager9* dev_manager) {
  if (!use_dxva2_)
    return NULL;
  CHECK(dev_manager != NULL);
  ScopedComPtr<IMFAttributes> attributes;

  // Create an attribute store with an initial size of 2.
  HRESULT hr = MFCreateAttributes(attributes.Receive(), 2);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create DXVA2 attributes for source reader";
    return NULL;
  }
  hr = attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, dev_manager);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set D3D9 manager to attribute";
    return NULL;
  }
  hr = attributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set DISABLE_DXVA to false";
    return NULL;
  }
  return attributes.Detach();
}

bool MFDecoder::SelectVideoStreamOnly() {
  CHECK(reader_ != NULL);
  HRESULT hr;
  for (DWORD stream_index = 0; ; stream_index++) {
    ScopedComPtr<IMFMediaType> media_type;
    hr = reader_->GetCurrentMediaType(stream_index, media_type.Receive());
    if (SUCCEEDED(hr)) {
      GUID major_type;
      hr = media_type->GetMajorType(&major_type);
      if (FAILED(hr)) {
        LOG(ERROR) << "Could not determine major type for stream "
                   << stream_index;
        return false;
      }
      if (major_type != MFMediaType_Video) {
        // Deselect any non-video streams.
        hr = reader_->SetStreamSelection(stream_index, FALSE);
        if (FAILED(hr)) {
          LOG(ERROR) << "Could not deselect stream " << stream_index;
          return false;
        }
      } else {
        // Ensure that the video stream is selected.
        hr = reader_->SetStreamSelection(stream_index, TRUE);
        if (FAILED(hr)) {
          LOG(ERROR) << "Could not select video stream " << stream_index;
          return false;
        }
        video_stream_index_ = stream_index;
        LOG(INFO) << "Video stream is at " << video_stream_index_;
      }
    } else if (hr == MF_E_INVALIDSTREAMNUMBER) {
      break;  // No more streams, quit.
    } else {
      LOG(ERROR) << "Error occurred while getting stream " << stream_index;
      return false;
    }
  }  // end of for-loop
  return video_stream_index_ >= 0;
}

bool MFDecoder::InitVideoInfo(IDirect3DDeviceManager9* dev_manager) {
  CHECK(reader_ != NULL);
  CHECK_GE(video_stream_index_, 0);
  ScopedComPtr<IMFMediaType> video_type;
  HRESULT hr = reader_->GetCurrentMediaType(video_stream_index_,
                                            video_type.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "InitVideoInfo: Failed to get video stream";
    return false;
  }
  GUID video_subtype;
  hr = video_type->GetGUID(MF_MT_SUBTYPE, &video_subtype);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to determine video subtype";
    return false;
  } else {
    if (video_subtype == MFVideoFormat_H264) {
      LOG(INFO) << "Video subtype is H.264";
    } else {
      LOG(INFO) << "Video subtype is NOT H.264";
    }
  }
  hr = MFGetAttributeSize(video_type, MF_MT_FRAME_SIZE,
                          reinterpret_cast<UINT32*>(&width_),
                          reinterpret_cast<UINT32*>(&height_));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to determine frame size";
    return false;
  } else {
    LOG(INFO) << "Video width: " << width_ << ", height: " << height_;
  }

  // Try to change to YV12 output format.
  const GUID kOutputVideoSubtype = MFVideoFormat_YV12;
  ScopedComPtr<IMFMediaType> output_video_format;
  hr = MFCreateMediaType(output_video_format.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create a IMFMediaType object for video output";
    return false;
  }
  if (SUCCEEDED(hr))
    hr = output_video_format->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (SUCCEEDED(hr))
    hr = output_video_format->SetGUID(MF_MT_SUBTYPE, kOutputVideoSubtype);
  if (SUCCEEDED(hr)) {
    hr = MFSetAttributeSize(output_video_format, MF_MT_FRAME_SIZE, width_,
                            height_);
  }
  if (SUCCEEDED(hr)) {
    hr = reader_->SetCurrentMediaType(video_stream_index_,
                                      NULL,                 // Reserved.
                                      output_video_format);
  }
  if (SUCCEEDED(hr)) {
    hr = MFGetStrideForBitmapInfoHeader(
        kOutputVideoSubtype.Data1,
        width_,
        reinterpret_cast<LONG*>(&mfbuffer_stride_));
  }
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to change output video format and determine stride";
    return false;
  } else {
    LOG(INFO) << "IMFMediaBuffer stride: " << mfbuffer_stride_;
  }

  // Send a message to the decoder to tell it to use DXVA2.
  if (use_dxva2_) {
    // Call GetServiceForStream to get the interface to the video decoder.
    ScopedComPtr<IMFTransform> video_decoder;
    hr = reader_->GetServiceForStream(video_stream_index_, GUID_NULL,
                                     IID_PPV_ARGS(video_decoder.Receive()));
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to obtain interface to decoder";
      return false;
    } else {
      hr = video_decoder->ProcessMessage(
          MFT_MESSAGE_SET_D3D_MANAGER,
          reinterpret_cast<ULONG_PTR>(dev_manager));
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to send DXVA message to decoder";
        return false;
      }
    }
  }
  return true;
}

}  // namespace media
