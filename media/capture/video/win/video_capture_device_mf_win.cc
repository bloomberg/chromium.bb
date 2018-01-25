// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_mf_win.h"

#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>
#include <wincodec.h>

#include <thread>
#include <utility>

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "media/capture/video/win/video_capture_device_utils_win.h"

using base::win::ScopedCoMem;
using Microsoft::WRL::ComPtr;
using base::Location;

namespace media {

namespace {
class MFPhotoCallback final
    : public base::RefCountedThreadSafe<MFPhotoCallback>,
      public IMFCaptureEngineOnSampleCallback {
 public:
  MFPhotoCallback(VideoCaptureDevice::TakePhotoCallback callback,
                  VideoCaptureFormat format)
      : callback_(std::move(callback)), format_(format) {}

  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    if (riid == IID_IUnknown || riid == IID_IMFCaptureEngineOnSampleCallback) {
      AddRef();
      *object = static_cast<IMFCaptureEngineOnSampleCallback*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MFPhotoCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MFPhotoCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnSample)(IMFSample* sample) override {
    if (!sample)
      return S_OK;

    DWORD buffer_count = 0;
    sample->GetBufferCount(&buffer_count);

    for (DWORD i = 0; i < buffer_count; ++i) {
      ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, buffer.GetAddressOf());
      if (!buffer)
        continue;

      BYTE* data = nullptr;
      DWORD max_length = 0;
      DWORD length = 0;
      buffer->Lock(&data, &max_length, &length);
      mojom::BlobPtr blob = Blobify(data, length, format_);
      buffer->Unlock();
      if (blob) {
        std::move(callback_).Run(std::move(blob));
        // What is it supposed to mean if there is more than one buffer sent to
        // us as a response to requesting a single still image? Are we supposed
        // to somehow concatenate the buffers? Or is it safe to ignore extra
        // buffers? For now, we ignore extra buffers.
        break;
      }
    }
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFPhotoCallback>;
  ~MFPhotoCallback() = default;

  VideoCaptureDevice::TakePhotoCallback callback_;
  const VideoCaptureFormat format_;

  DISALLOW_COPY_AND_ASSIGN(MFPhotoCallback);
};

scoped_refptr<IMFCaptureEngineOnSampleCallback> CreateMFPhotoCallback(
    VideoCaptureDevice::TakePhotoCallback callback,
    VideoCaptureFormat format) {
  return scoped_refptr<IMFCaptureEngineOnSampleCallback>(
      new MFPhotoCallback(std::move(callback), format));
}
}  // namespace

// Since IMFCaptureEngine performs the video decoding itself, and Chromium uses
// I420 at the other end of the pipe, video output format is forced to I420.
static const GUID kSinkMFVideoFormat = MFVideoFormat_I420;
static const VideoPixelFormat kSinkVideoPixelFormat = PIXEL_FORMAT_I420;

void LogError(const Location& from_here, HRESULT hr) {
  // Log directly the error to help debugging browser functional test.
  // TODO(alaoui.rda@gmail.com): condition with NDEBUG after 858138 CL revert
  LOG(ERROR) << from_here.ToString()
             << " hr = " << logging::SystemErrorCodeToString(hr);
}

static bool GetFrameSizeFromMediaType(IMFMediaType* type,
                                      gfx::Size* frame_size) {
  UINT32 width32, height32;
  if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width32, &height32)))
    return false;
  frame_size->SetSize(width32, height32);
  return true;
}

static bool GetFrameRateFromMediaType(IMFMediaType* type, float* frame_rate) {
  UINT32 numerator, denominator;
  if (FAILED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator,
                                 &denominator)) ||
      !denominator) {
    return false;
  }
  *frame_rate = static_cast<float>(numerator) / denominator;
  return true;
}

static bool GetFormatFromMediaType(IMFMediaType* type,
                                   bool photo,
                                   VideoCaptureFormat* format) {
  GUID major_type_guid;
  if (FAILED(type->GetGUID(MF_MT_MAJOR_TYPE, &major_type_guid)) ||
      (major_type_guid != MFMediaType_Image &&
       (photo || !GetFrameRateFromMediaType(type, &format->frame_rate)))) {
    return false;
  }

  GUID sub_type_guid;
  if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &sub_type_guid)) ||
      !GetFrameSizeFromMediaType(type, &format->frame_size) ||
      !VideoCaptureDeviceMFWin::FormatFromGuid(sub_type_guid,
                                               &format->pixel_format)) {
    return false;
  }

  return true;
}

static HRESULT CopyAttribute(IMFAttributes* source_attributes,
                             IMFAttributes* destination_attributes,
                             const GUID& key) {
  PROPVARIANT var;
  PropVariantInit(&var);
  HRESULT hr = source_attributes->GetItem(key, &var);
  if (FAILED(hr))
    return hr;

  hr = destination_attributes->SetItem(key, var);
  PropVariantClear(&var);
  return hr;
}

static HRESULT ConvertToPhotoSinkMediaType(
    IMFMediaType* source_media_type,
    IMFMediaType* destination_media_type) {
  HRESULT hr =
      destination_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Image);
  if (FAILED(hr))
    return hr;

  hr = destination_media_type->SetGUID(MF_MT_SUBTYPE, GUID_ContainerFormatJpeg);
  if (FAILED(hr))
    return hr;

  return CopyAttribute(source_media_type, destination_media_type,
                       MF_MT_FRAME_SIZE);
}

static HRESULT ConvertToVideoSinkMediaType(IMFMediaType* source_media_type,
                                           IMFMediaType* sink_media_type) {
  HRESULT hr = sink_media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr))
    return hr;

  hr = sink_media_type->SetGUID(MF_MT_SUBTYPE, kSinkMFVideoFormat);
  if (FAILED(hr))
    return hr;

  hr = CopyAttribute(source_media_type, sink_media_type, MF_MT_FRAME_SIZE);
  if (FAILED(hr))
    return hr;

  hr = CopyAttribute(source_media_type, sink_media_type, MF_MT_FRAME_RATE);
  if (FAILED(hr))
    return hr;

  hr = CopyAttribute(source_media_type, sink_media_type,
                     MF_MT_PIXEL_ASPECT_RATIO);
  if (FAILED(hr))
    return hr;

  return CopyAttribute(source_media_type, sink_media_type,
                       MF_MT_INTERLACE_MODE);
}

static const CapabilityWin& GetBestMatchedPhotoCapability(
    ComPtr<IMFMediaType> current_media_type,
    gfx::Size requested_size,
    const CapabilityList& capabilities) {
  gfx::Size current_size;
  GetFrameSizeFromMediaType(current_media_type.Get(), &current_size);

  int requested_height = requested_size.height() > 0 ? requested_size.height()
                                                     : current_size.height();
  int requested_width = requested_size.width() > 0 ? requested_size.width()
                                                   : current_size.width();

  const CapabilityWin* best_match = &(*capabilities.begin());
  for (const CapabilityWin& capability : capabilities) {
    int height = capability.supported_format.frame_size.height();
    int width = capability.supported_format.frame_size.width();
    int best_height = best_match->supported_format.frame_size.height();
    int best_width = best_match->supported_format.frame_size.width();

    if (std::abs(height - requested_height) <= std::abs(height - best_height) &&
        std::abs(width - requested_width) <= std::abs(width - best_width)) {
      best_match = &capability;
    }
  }
  return *best_match;
}

HRESULT GetAvailableDeviceMediaType(IMFCaptureSource* source,
                                    DWORD stream_index,
                                    DWORD media_type_index,
                                    IMFMediaType** type) {
  HRESULT hr;
  // Rarely, for some unknown reason, GetAvailableDeviceMediaType returns an
  // undocumented MF_E_INVALIDREQUEST. Retrying solves the issue.
  int retry_count = 0;
  do {
    hr = source->GetAvailableDeviceMediaType(stream_index, media_type_index,
                                             type);
    if (FAILED(hr))
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));

    // Give up after ~10 seconds
  } while (hr == MF_E_INVALIDREQUEST && retry_count++ < 200);

  return hr;
}

HRESULT FillCapabilities(DWORD stream,
                         IMFCaptureSource* source,
                         bool photo,
                         CapabilityList* capabilities) {
  DWORD media_type_index = 0;
  ComPtr<IMFMediaType> type;
  HRESULT hr;

  while (
      SUCCEEDED(hr = GetAvailableDeviceMediaType(
                    source, stream, media_type_index, type.GetAddressOf()))) {
    VideoCaptureFormat format;
    if (GetFormatFromMediaType(type.Get(), photo, &format))
      capabilities->emplace_back(media_type_index, format);
    type.Reset();
    ++media_type_index;
  }

  return (hr == MF_E_NO_MORE_TYPES) ? S_OK : hr;
}

class MFVideoCallback final
    : public base::RefCountedThreadSafe<MFVideoCallback>,
      public IMFCaptureEngineOnSampleCallback,
      public IMFCaptureEngineOnEventCallback {
 public:
  MFVideoCallback(VideoCaptureDeviceMFWin* observer) : observer_(observer) {}

  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    HRESULT hr = E_NOINTERFACE;
    if (riid == IID_IUnknown) {
      *object = this;
      hr = S_OK;
    } else if (riid == IID_IMFCaptureEngineOnSampleCallback) {
      *object = static_cast<IMFCaptureEngineOnSampleCallback*>(this);
      hr = S_OK;
    } else if (riid == IID_IMFCaptureEngineOnEventCallback) {
      *object = static_cast<IMFCaptureEngineOnEventCallback*>(this);
      hr = S_OK;
    }
    if (SUCCEEDED(hr))
      AddRef();

    return hr;
  }

  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MFVideoCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MFVideoCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnEvent)(IMFMediaEvent* media_event) override {
    observer_->OnEvent(media_event);
    return S_OK;
  }

  STDMETHOD(OnSample)(IMFSample* sample) override {
    base::TimeTicks reference_time(base::TimeTicks::Now());

    LONGLONG raw_time_stamp = 0;
    sample->GetSampleTime(&raw_time_stamp);
    base::TimeDelta timestamp =
        base::TimeDelta::FromMicroseconds(raw_time_stamp / 10);
    if (!sample) {
      observer_->OnIncomingCapturedData(NULL, 0, 0, reference_time, timestamp);
      return S_OK;
    }

    DWORD count = 0;
    sample->GetBufferCount(&count);

    for (DWORD i = 0; i < count; ++i) {
      ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, buffer.GetAddressOf());
      if (buffer) {
        DWORD length = 0, max_length = 0;
        BYTE* data = NULL;
        buffer->Lock(&data, &max_length, &length);
        observer_->OnIncomingCapturedData(data, length, GetCameraRotation(),
                                          reference_time, timestamp);
        buffer->Unlock();
      }
    }
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFVideoCallback>;
  ~MFVideoCallback() {}
  VideoCaptureDeviceMFWin* observer_;
};

// static

bool VideoCaptureDeviceMFWin::FormatFromGuid(const GUID& guid,
                                             VideoPixelFormat* format) {
  struct {
    const GUID& guid;
    const VideoPixelFormat format;
  } static const kFormatMap[] = {
      {MFVideoFormat_I420, kSinkVideoPixelFormat},
      {MFVideoFormat_YUY2, kSinkVideoPixelFormat},
      {MFVideoFormat_UYVY, kSinkVideoPixelFormat},
      {MFVideoFormat_RGB24, kSinkVideoPixelFormat},
      {MFVideoFormat_RGB32, kSinkVideoPixelFormat},
      {MFVideoFormat_ARGB32, kSinkVideoPixelFormat},
      {MFVideoFormat_MJPG, kSinkVideoPixelFormat},
      {MFVideoFormat_NV12, kSinkVideoPixelFormat},
      {MFVideoFormat_YV12, kSinkVideoPixelFormat},
      {kMediaSubTypeY16, kSinkVideoPixelFormat},
      {kMediaSubTypeZ16, kSinkVideoPixelFormat},
      {kMediaSubTypeINVZ, kSinkVideoPixelFormat},

      {GUID_ContainerFormatJpeg, PIXEL_FORMAT_MJPEG}};

  for (const auto& kFormat : kFormatMap) {
    if (kFormat.guid == guid) {
      *format = kFormat.format;
      return true;
    }
  }

  return false;
}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : descriptor_(device_descriptor),
      create_mf_photo_callback_(base::BindRepeating(&CreateMFPhotoCallback)),
      is_started_(false),
      video_stream_index_(static_cast<DWORD>(
          MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_PREVIEW)),
      photo_stream_index_(static_cast<DWORD>(
          MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO)),
      use_photo_stream_to_take_photo_(false) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VideoCaptureDeviceMFWin::~VideoCaptureDeviceMFWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool VideoCaptureDeviceMFWin::Init(const ComPtr<IMFMediaSource>& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!engine_);

  HRESULT hr = S_OK;
  ComPtr<IMFAttributes> attributes;
  ComPtr<IMFCaptureEngineClassFactory> capture_engine_class_factory;
  MFCreateAttributes(attributes.GetAddressOf(), 1);
  DCHECK(attributes);

  hr = CoCreateInstance(
      CLSID_MFCaptureEngineClassFactory, NULL, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS(capture_engine_class_factory.GetAddressOf()));
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }
  hr = capture_engine_class_factory->CreateInstance(
      CLSID_MFCaptureEngine, IID_PPV_ARGS(engine_.GetAddressOf()));
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  video_callback_ = new MFVideoCallback(this);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  hr = engine_->Initialize(video_callback_.get(), attributes.Get(), nullptr,
                           source.Get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return false;
  }

  return true;
}

void VideoCaptureDeviceMFWin::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock lock(lock_);

  client_ = std::move(client);
  DCHECK_EQ(false, is_started_);

  if (!engine_) {
    OnError(FROM_HERE, E_FAIL);
    return;
  }

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = engine_->GetSource(source.GetAddressOf());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  DWORD stream_count = 0;
  hr = source->GetDeviceStreamCount(&stream_count);
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  LOG(WARNING) << "Device stream count=" << stream_count;

  CapabilityList photo_capabilities;
  hr = FillCapabilities(photo_stream_index_, source.Get(), true,
                        &photo_capabilities);
  if (FAILED(hr)) {
    LOG(WARNING)
        << "MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO failed with hr="
        << logging::SystemErrorCodeToString(hr);
    photo_stream_index_ = 0;
    hr = FillCapabilities(photo_stream_index_, source.Get(), true,
                          &photo_capabilities);
  } else
    LOG(WARNING)
        << "MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_PHOTO succeeded";

  if (FAILED(hr)) {
    LOG(WARNING) << "Photo stream index 0 failed";
    OnError(FROM_HERE, hr);
    return;
  } else
    LOG(WARNING) << "Photo stream index 0 succeeded";

  use_photo_stream_to_take_photo_ = !photo_capabilities.empty();

  CapabilityList video_capabilities;
  hr = FillCapabilities(video_stream_index_, source.Get(), false,
                        &video_capabilities);
  if (FAILED(hr)) {
    LOG(WARNING) << "MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_"
                    "PREVIEW failed with hr="
                 << logging::SystemErrorCodeToString(hr);
    video_stream_index_ = 0;
    hr = FillCapabilities(video_stream_index_, source.Get(), false,
                          &video_capabilities);
  } else
    LOG(WARNING) << "MF_CAPTURE_ENGINE_PREFERRED_SOURCE_STREAM_FOR_VIDEO_"
                    "PREVIEW succeeded";

  if (FAILED(hr)) {
    LOG(WARNING) << "Video stream index 0 failed";
    OnError(FROM_HERE, hr);
    return;
  } else
    LOG(WARNING) << "Video stream index 0 succeeded";

  const CapabilityWin found_capability =
      GetBestMatchedCapability(params.requested_format, video_capabilities);
  ComPtr<IMFMediaType> source_video_media_type;
  hr = GetAvailableDeviceMediaType(source.Get(), video_stream_index_,
                                   found_capability.stream_index,
                                   source_video_media_type.GetAddressOf());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  hr = source->SetCurrentDeviceMediaType(video_stream_index_,
                                         source_video_media_type.Get());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCaptureSink> sink;
  hr = engine_->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW,
                        sink.GetAddressOf());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCapturePreviewSink> preview_sink;
  hr = sink->QueryInterface(IID_PPV_ARGS(preview_sink.GetAddressOf()));
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  hr = preview_sink->RemoveAllStreams();
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> sink_video_media_type;
  hr = MFCreateMediaType(sink_video_media_type.GetAddressOf());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  hr = ConvertToVideoSinkMediaType(source_video_media_type.Get(),
                                   sink_video_media_type.Get());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  DWORD dw_sink_stream_index = 0;
  hr = preview_sink->AddStream(video_stream_index_, sink_video_media_type.Get(),
                               NULL, &dw_sink_stream_index);
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  hr = preview_sink->SetSampleCallback(dw_sink_stream_index,
                                       video_callback_.get());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  hr = engine_->StartPreview();
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  capture_video_format_ = found_capability.supported_format;

  client_->OnStarted();
  is_started_ = true;
}

void VideoCaptureDeviceMFWin::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);

  if (is_started_ && engine_)
    engine_->StopPreview();
  is_started_ = false;

  client_.reset();
}

void VideoCaptureDeviceMFWin::TakePhoto(TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(lock_);

  if (!is_started_)
    return;

  if (!use_photo_stream_to_take_photo_) {
    video_stream_take_photo_callbacks_.push(std::move(callback));
    return;
  }

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = engine_->GetSource(source.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> source_media_type;
  hr = source->GetCurrentDeviceMediaType(photo_stream_index_,
                                         source_media_type.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> sink_media_type;
  hr = MFCreateMediaType(sink_media_type.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = ConvertToPhotoSinkMediaType(source_media_type.Get(),
                                   sink_media_type.Get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  VideoCaptureFormat format;
  hr = GetFormatFromMediaType(sink_media_type.Get(), true, &format) ? S_OK
                                                                    : E_FAIL;
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCaptureSink> sink;
  hr = engine_->GetSink(MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, sink.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFCapturePhotoSink> photo_sink;
  hr = sink->QueryInterface(IID_PPV_ARGS(photo_sink.GetAddressOf()));
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = photo_sink->RemoveAllStreams();
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  DWORD dw_sink_stream_index = 0;
  hr = photo_sink->AddStream(photo_stream_index_, sink_media_type.Get(), NULL,
                             &dw_sink_stream_index);
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  scoped_refptr<IMFCaptureEngineOnSampleCallback> photo_callback =
      create_mf_photo_callback_.Run(std::move(callback), format);
  hr = photo_sink->SetSampleCallback(photo_callback.get());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = engine_->TakePhoto();
  if (FAILED(hr))
    LogError(FROM_HERE, hr);
}

void VideoCaptureDeviceMFWin::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started_)
    return;

  ComPtr<IMFCaptureSource> source;
  HRESULT hr = engine_->GetSource(source.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  CapabilityList capabilities;
  if (use_photo_stream_to_take_photo_)
    hr = FillCapabilities(photo_stream_index_, source.Get(), true,
                          &capabilities);

  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> current_media_type;
  hr = source->GetCurrentDeviceMediaType(use_photo_stream_to_take_photo_
                                             ? photo_stream_index_
                                             : video_stream_index_,
                                         current_media_type.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  auto photo_capabilities = mojom::PhotoState::New();
  gfx::Size current_size;
  GetFrameSizeFromMediaType(current_media_type.Get(), &current_size);

  gfx::Size min_size = gfx::Size(current_size.width(), current_size.height());
  gfx::Size max_size = gfx::Size(current_size.width(), current_size.height());
  for (const CapabilityWin& capability : capabilities) {
    min_size.SetToMin(capability.supported_format.frame_size);
    max_size.SetToMax(capability.supported_format.frame_size);
  }

  photo_capabilities->height = mojom::Range::New(
      max_size.height(), min_size.height(), current_size.height(), 1);
  photo_capabilities->width = mojom::Range::New(
      max_size.width(), min_size.width(), current_size.width(), 1);

  photo_capabilities->exposure_compensation = mojom::Range::New();
  photo_capabilities->color_temperature = mojom::Range::New();
  photo_capabilities->iso = mojom::Range::New();
  photo_capabilities->brightness = mojom::Range::New();
  photo_capabilities->contrast = mojom::Range::New();
  photo_capabilities->saturation = mojom::Range::New();
  photo_capabilities->sharpness = mojom::Range::New();
  photo_capabilities->zoom = mojom::Range::New();
  photo_capabilities->red_eye_reduction = mojom::RedEyeReduction::NEVER;

  photo_capabilities->torch = false;
  std::move(callback).Run(std::move(photo_capabilities));
}

void VideoCaptureDeviceMFWin::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_started_)
    return;

  HRESULT hr = S_OK;
  ComPtr<IMFCaptureSource> source;
  hr = engine_->GetSource(source.GetAddressOf());

  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  if (use_photo_stream_to_take_photo_ &&
      (settings->has_height || settings->has_width)) {
    CapabilityList capabilities;
    hr = FillCapabilities(photo_stream_index_, source.Get(), true,
                          &capabilities);

    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }

    ComPtr<IMFMediaType> current_source_media_type;
    hr = source->GetCurrentDeviceMediaType(
        photo_stream_index_, current_source_media_type.GetAddressOf());

    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }

    gfx::Size requested_size = gfx::Size();
    if (settings->has_height)
      requested_size.set_height(settings->height);

    if (settings->has_width)
      requested_size.set_width(settings->width);

    const CapabilityWin best_match = GetBestMatchedPhotoCapability(
        current_source_media_type, requested_size, capabilities);

    ComPtr<IMFMediaType> best_source_media_type;
    hr = GetAvailableDeviceMediaType(source.Get(), photo_stream_index_,
                                     best_match.stream_index,
                                     best_source_media_type.GetAddressOf());
    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }

    hr = source->SetCurrentDeviceMediaType(photo_stream_index_,
                                           best_source_media_type.Get());
    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }
  }

  std::move(callback).Run(true);
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    int rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  base::AutoLock lock(lock_);

  if (!data)
    return;

  if (client_.get()) {
    client_->OnIncomingCapturedData(data, length, capture_video_format_,
                                    rotation, reference_time, timestamp);
  }

  while (!video_stream_take_photo_callbacks_.empty()) {
    TakePhotoCallback cb =
        std::move(video_stream_take_photo_callbacks_.front());
    video_stream_take_photo_callbacks_.pop();

    mojom::BlobPtr blob = Blobify(data, length, capture_video_format_);
    if (!blob)
      continue;

    std::move(cb).Run(std::move(blob));
  }
}

void VideoCaptureDeviceMFWin::OnEvent(IMFMediaEvent* media_event) {
  base::AutoLock lock(lock_);

  HRESULT hr;
  media_event->GetStatus(&hr);

  if (FAILED(hr))
    OnError(FROM_HERE, hr);
}

void VideoCaptureDeviceMFWin::OnError(const Location& from_here, HRESULT hr) {
  // Log directly the error to help debugging browser functional test.
  // TODO(alaoui.rda@gmail.com): remove after 858138 CL revert
  LogError(from_here, hr);

  if (client_.get()) {
    client_->OnError(
        from_here,
        base::StringPrintf("VideoCaptureDeviceMFWin: %s",
                           logging::SystemErrorCodeToString(hr).c_str()));
  }
}

}  // namespace media