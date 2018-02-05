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
#include "media/capture/mojo/image_capture_types.h"
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
  DPLOG(ERROR) << from_here.ToString()
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

HRESULT ExecuteHresultCallbackWithRetries(
    base::RepeatingCallback<HRESULT()> callback) {
  // Retry callback execution on MF_E_INVALIDREQUEST.
  // MF_E_INVALIDREQUEST is not documented in MediaFoundation documentation.
  // It could mean that MediaFoundation or the underlying device can be in a
  // state that reject these calls. Since MediaFoundation gives no intel about
  // that state beginning and ending (i.e. via some kind of event), we retry the
  // call until it succeed.
  HRESULT hr;
  int retry_count = 0;
  do {
    hr = callback.Run();
    if (FAILED(hr))
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(50));

    // Give up after ~10 seconds
  } while (hr == MF_E_INVALIDREQUEST && retry_count++ < 200);

  return hr;
}

HRESULT GetDeviceStreamCount(IMFCaptureSource* source, DWORD* count) {
  // Sometimes, GetDeviceStreamCount returns an
  // undocumented MF_E_INVALIDREQUEST. Retrying solves the issue.
  return ExecuteHresultCallbackWithRetries(base::BindRepeating(
      [](IMFCaptureSource* source, DWORD* count) {
        return source->GetDeviceStreamCount(count);
      },
      source, count));
}

HRESULT GetDeviceStreamCategory(
    IMFCaptureSource* source,
    DWORD stream_index,
    MF_CAPTURE_ENGINE_STREAM_CATEGORY* stream_category) {
  // We believe that GetDeviceStreamCategory could be affected by the same
  // behaviour of GetDeviceStreamCount and GetAvailableDeviceMediaType
  return ExecuteHresultCallbackWithRetries(base::BindRepeating(
      [](IMFCaptureSource* source, DWORD stream_index,
         MF_CAPTURE_ENGINE_STREAM_CATEGORY* stream_category) {
        return source->GetDeviceStreamCategory(stream_index, stream_category);
      },
      source, stream_index, stream_category));
}

HRESULT GetAvailableDeviceMediaType(IMFCaptureSource* source,
                                    DWORD stream_index,
                                    DWORD media_type_index,
                                    IMFMediaType** type) {
  // Rarely, for some unknown reason, GetAvailableDeviceMediaType returns an
  // undocumented MF_E_INVALIDREQUEST. Retrying solves the issue.
  return ExecuteHresultCallbackWithRetries(base::BindRepeating(
      [](IMFCaptureSource* source, DWORD stream_index, DWORD media_type_index,
         IMFMediaType** type) {
        return source->GetAvailableDeviceMediaType(stream_index,
                                                   media_type_index, type);
      },
      source, stream_index, media_type_index, type));
}

HRESULT FillCapabilities(IMFCaptureSource* source,
                         bool photo,
                         CapabilityList* capabilities) {
  DWORD stream_count = 0;
  HRESULT hr = GetDeviceStreamCount(source, &stream_count);
  if (FAILED(hr))
    return hr;

  for (DWORD stream_index = 0; stream_index < stream_count; stream_index++) {
    MF_CAPTURE_ENGINE_STREAM_CATEGORY stream_category;
    hr = GetDeviceStreamCategory(source, stream_index, &stream_category);
    if (FAILED(hr))
      return hr;

    if ((photo && stream_category !=
                      MF_CAPTURE_ENGINE_STREAM_CATEGORY_PHOTO_INDEPENDENT) ||
        (!photo &&
         stream_category != MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_PREVIEW &&
         stream_category != MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_CAPTURE)) {
      continue;
    }

    DWORD media_type_index = 0;
    ComPtr<IMFMediaType> type;
    while (SUCCEEDED(hr = GetAvailableDeviceMediaType(source, stream_index,
                                                      media_type_index,
                                                      type.GetAddressOf()))) {
      VideoCaptureFormat format;
      if (GetFormatFromMediaType(type.Get(), photo, &format))
        capabilities->emplace_back(media_type_index, format, stream_index);
      type.Reset();
      ++media_type_index;
    }
    if (hr == MF_E_NO_MORE_TYPES) {
      hr = S_OK;
    }
    if (FAILED(hr)) {
      return hr;
    }
  }

  return hr;
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
      is_started_(false) {
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

  hr = FillCapabilities(source.Get(), true, &photo_capabilities_);
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  if (!photo_capabilities_.empty()) {
    selected_photo_capability_.reset(
        new CapabilityWin(photo_capabilities_.front()));
  }

  CapabilityList video_capabilities;
  hr = FillCapabilities(source.Get(), false, &video_capabilities);
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  if (video_capabilities.empty()) {
    OnError(FROM_HERE, "No video capability found");
    return;
  }

  const CapabilityWin best_match_video_capability =
      GetBestMatchedCapability(params.requested_format, video_capabilities);
  ComPtr<IMFMediaType> source_video_media_type;
  hr = GetAvailableDeviceMediaType(source.Get(),
                                   best_match_video_capability.stream_index,
                                   best_match_video_capability.media_type_index,
                                   source_video_media_type.GetAddressOf());
  if (FAILED(hr)) {
    OnError(FROM_HERE, hr);
    return;
  }

  hr = source->SetCurrentDeviceMediaType(
      best_match_video_capability.stream_index, source_video_media_type.Get());
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
  hr = preview_sink->AddStream(best_match_video_capability.stream_index,
                               sink_video_media_type.Get(), NULL,
                               &dw_sink_stream_index);
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

  selected_video_capability_.reset(
      new CapabilityWin(best_match_video_capability));

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

  if (!selected_photo_capability_) {
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
  hr = GetAvailableDeviceMediaType(source.Get(),
                                   selected_photo_capability_->stream_index,
                                   selected_photo_capability_->media_type_index,
                                   source_media_type.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  hr = source->SetCurrentDeviceMediaType(
      selected_photo_capability_->stream_index, source_media_type.Get());
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
  hr =
      photo_sink->AddStream(selected_photo_capability_->stream_index,
                            sink_media_type.Get(), NULL, &dw_sink_stream_index);
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

  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  ComPtr<IMFMediaType> current_media_type;
  hr = source->GetCurrentDeviceMediaType(
      selected_photo_capability_ ? selected_photo_capability_->stream_index
                                 : selected_video_capability_->stream_index,
      current_media_type.GetAddressOf());
  if (FAILED(hr)) {
    LogError(FROM_HERE, hr);
    return;
  }

  auto photo_capabilities = mojo::CreateEmptyPhotoState();
  gfx::Size current_size;
  GetFrameSizeFromMediaType(current_media_type.Get(), &current_size);

  gfx::Size min_size = gfx::Size(current_size.width(), current_size.height());
  gfx::Size max_size = gfx::Size(current_size.width(), current_size.height());
  for (const CapabilityWin& capability : photo_capabilities_) {
    min_size.SetToMin(capability.supported_format.frame_size);
    max_size.SetToMax(capability.supported_format.frame_size);
  }

  photo_capabilities->height = mojom::Range::New(
      max_size.height(), min_size.height(), current_size.height(), 1);
  photo_capabilities->width = mojom::Range::New(
      max_size.width(), min_size.width(), current_size.width(), 1);

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

  if (!photo_capabilities_.empty() &&
      (settings->has_height || settings->has_width)) {
    if (FAILED(hr)) {
      LogError(FROM_HERE, hr);
      return;
    }

    ComPtr<IMFMediaType> current_source_media_type;
    hr = source->GetCurrentDeviceMediaType(
        selected_photo_capability_->stream_index,
        current_source_media_type.GetAddressOf());

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
        current_source_media_type, requested_size, photo_capabilities_);
    selected_photo_capability_.reset(new CapabilityWin(best_match));
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
    client_->OnIncomingCapturedData(
        data, length, selected_video_capability_->supported_format, rotation,
        reference_time, timestamp);
  }

  while (!video_stream_take_photo_callbacks_.empty()) {
    TakePhotoCallback cb =
        std::move(video_stream_take_photo_callbacks_.front());
    video_stream_take_photo_callbacks_.pop();

    mojom::BlobPtr blob =
        Blobify(data, length, selected_video_capability_->supported_format);
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
  OnError(from_here, logging::SystemErrorCodeToString(hr).c_str());
}

void VideoCaptureDeviceMFWin::OnError(const Location& from_here,
                                      const char* message) {
  if (!client_.get())
    return;

  client_->OnError(from_here,
                   base::StringPrintf("VideoCaptureDeviceMFWin: %s", message));
}

}  // namespace media