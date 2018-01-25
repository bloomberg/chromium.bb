// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_mf_win.h"

#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>

#include <utility>

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "media/capture/video/win/video_capture_device_utils_win.h"

using base::win::ScopedCoMem;

namespace media {

static bool GetFrameSize(IMFMediaType* type, gfx::Size* frame_size) {
  UINT32 width32, height32;
  if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width32, &height32)))
    return false;
  frame_size->SetSize(width32, height32);
  return true;
}

static bool GetFrameRate(IMFMediaType* type, float* frame_rate) {
  UINT32 numerator, denominator;
  if (FAILED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator,
                                 &denominator)) ||
      !denominator) {
    return false;
  }
  *frame_rate = static_cast<float>(numerator) / denominator;
  return true;
}

static bool FillFormat(IMFMediaType* type, VideoCaptureFormat* format) {
  GUID type_guid;
  if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &type_guid)) ||
      !GetFrameSize(type, &format->frame_size) ||
      !GetFrameRate(type, &format->frame_rate) ||
      !VideoCaptureDeviceMFWin::FormatFromGuid(type_guid,
                                               &format->pixel_format)) {
    return false;
  }

  return true;
}

HRESULT FillCapabilities(IMFSourceReader* source,
                         CapabilityList* capabilities) {
  DWORD stream_index = 0;
  Microsoft::WRL::ComPtr<IMFMediaType> type;
  HRESULT hr;
  while (SUCCEEDED(hr = source->GetNativeMediaType(
                       kFirstVideoStream, stream_index, type.GetAddressOf()))) {
    VideoCaptureFormat format;
    if (FillFormat(type.Get(), &format))
      capabilities->emplace_back(stream_index, format);
    type.Reset();
    ++stream_index;
  }

  if (capabilities->empty() && (SUCCEEDED(hr) || hr == MF_E_NO_MORE_TYPES))
    hr = HRESULT_FROM_WIN32(ERROR_EMPTY);

  return (hr == MF_E_NO_MORE_TYPES) ? S_OK : hr;
}

class MFReaderCallback final
    : public base::RefCountedThreadSafe<MFReaderCallback>,
      public IMFSourceReaderCallback {
 public:
  MFReaderCallback(VideoCaptureDeviceMFWin* observer)
      : observer_(observer), wait_event_(NULL) {}

  void SetSignalOnFlush(base::WaitableEvent* event) { wait_event_ = event; }

  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    if (riid != IID_IUnknown && riid != IID_IMFSourceReaderCallback)
      return E_NOINTERFACE;
    *object = static_cast<IMFSourceReaderCallback*>(this);
    AddRef();
    return S_OK;
  }

  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MFReaderCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MFReaderCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnReadSample)
  (HRESULT status,
   DWORD stream_index,
   DWORD stream_flags,
   LONGLONG raw_time_stamp,
   IMFSample* sample) override {
    base::TimeTicks reference_time(base::TimeTicks::Now());
    base::TimeDelta timestamp =
        base::TimeDelta::FromMicroseconds(raw_time_stamp / 10);
    if (!sample) {
      observer_->OnIncomingCapturedData(NULL, 0, 0, reference_time, timestamp);
      return S_OK;
    }

    DWORD count = 0;
    sample->GetBufferCount(&count);

    for (DWORD i = 0; i < count; ++i) {
      Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, buffer.GetAddressOf());
      if (buffer.Get()) {
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

  STDMETHOD(OnFlush)(DWORD stream_index) override {
    if (wait_event_) {
      wait_event_->Signal();
      wait_event_ = NULL;
    }
    return S_OK;
  }

  STDMETHOD(OnEvent)(DWORD stream_index, IMFMediaEvent* event) override {
    NOTIMPLEMENTED();
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<MFReaderCallback>;
  ~MFReaderCallback() {}

  VideoCaptureDeviceMFWin* observer_;
  base::WaitableEvent* wait_event_;
};

// static
bool VideoCaptureDeviceMFWin::FormatFromGuid(const GUID& guid,
                                             VideoPixelFormat* format) {
  struct {
    const GUID& guid;
    const VideoPixelFormat format;
  } static const kFormatMap[] = {
      {MFVideoFormat_I420, PIXEL_FORMAT_I420},
      {MFVideoFormat_YUY2, PIXEL_FORMAT_YUY2},
      {MFVideoFormat_UYVY, PIXEL_FORMAT_UYVY},
      {MFVideoFormat_RGB24, PIXEL_FORMAT_RGB24},
      {MFVideoFormat_ARGB32, PIXEL_FORMAT_ARGB},
      {MFVideoFormat_MJPG, PIXEL_FORMAT_MJPEG},
      {MFVideoFormat_YV12, PIXEL_FORMAT_YV12},
      {kMediaSubTypeY16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeZ16, PIXEL_FORMAT_Y16},
      {kMediaSubTypeINVZ, PIXEL_FORMAT_Y16},
  };

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
    : descriptor_(device_descriptor), capture_(0) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VideoCaptureDeviceMFWin::~VideoCaptureDeviceMFWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool VideoCaptureDeviceMFWin::Init(
    const Microsoft::WRL::ComPtr<IMFMediaSource>& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!reader_.Get());

  Microsoft::WRL::ComPtr<IMFAttributes> attributes;
  MFCreateAttributes(attributes.GetAddressOf(), 1);
  DCHECK(attributes.Get());

  callback_ = new MFReaderCallback(this);
  attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback_.get());

  return SUCCEEDED(MFCreateSourceReaderFromMediaSource(
      source.Get(), attributes.Get(), reader_.GetAddressOf()));
}

void VideoCaptureDeviceMFWin::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock lock(lock_);

  client_ = std::move(client);
  DCHECK_EQ(capture_, false);

  CapabilityList capabilities;
  HRESULT hr = S_OK;
  if (reader_.Get()) {
    hr = FillCapabilities(reader_.Get(), &capabilities);
    if (SUCCEEDED(hr)) {
      const CapabilityWin found_capability =
          GetBestMatchedCapability(params.requested_format, capabilities);
      Microsoft::WRL::ComPtr<IMFMediaType> type;
      hr = reader_->GetNativeMediaType(kFirstVideoStream,
                                       found_capability.stream_index,
                                       type.GetAddressOf());
      if (SUCCEEDED(hr)) {
        hr = reader_->SetCurrentMediaType(kFirstVideoStream, NULL, type.Get());
        if (SUCCEEDED(hr)) {
          hr =
              reader_->ReadSample(kFirstVideoStream, 0, NULL, NULL, NULL, NULL);
          if (SUCCEEDED(hr)) {
            capture_format_ = found_capability.supported_format;
            client_->OnStarted();
            capture_ = true;
            return;
          }
        }
      }
    }
  }

  OnError(FROM_HERE, hr);
}

void VideoCaptureDeviceMFWin::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::WaitableEvent flushed(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  const int kFlushTimeOutInMs = 1000;
  bool wait = false;
  {
    base::AutoLock lock(lock_);
    if (capture_) {
      capture_ = false;
      callback_->SetSignalOnFlush(&flushed);
      wait = SUCCEEDED(
          reader_->Flush(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS)));
      if (!wait) {
        callback_->SetSignalOnFlush(NULL);
      }
    }
    client_.reset();
  }

  // If the device has been unplugged, the Flush() won't trigger the event
  // and a timeout will happen.
  // TODO(tommi): Hook up the IMFMediaEventGenerator notifications API and
  // do not wait at all after getting MEVideoCaptureDeviceRemoved event.
  // See issue/226396.
  if (wait)
    flushed.TimedWait(base::TimeDelta::FromMilliseconds(kFlushTimeOutInMs));
}

void VideoCaptureDeviceMFWin::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    int rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  base::AutoLock lock(lock_);
  if (data && client_.get()) {
    client_->OnIncomingCapturedData(data, length, capture_format_, rotation,
                                    reference_time, timestamp);
  }

  if (capture_) {
    HRESULT hr =
        reader_->ReadSample(kFirstVideoStream, 0, NULL, NULL, NULL, NULL);
    if (FAILED(hr)) {
      // If running the *VideoCap* unit tests on repeat, this can sometimes
      // fail with HRESULT_FROM_WINHRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION).
      // It's not clear to me why this is, but it is possible that it has
      // something to do with this bug:
      // http://support.microsoft.com/kb/979567
      OnError(FROM_HERE, hr);
    }
  }
}

void VideoCaptureDeviceMFWin::OnError(const base::Location& from_here,
                                      HRESULT hr) {
  if (client_.get()) {
    client_->OnError(
        from_here,
        base::StringPrintf("VideoCaptureDeviceMFWin: %s",
                           logging::SystemErrorCodeToString(hr).c_str()));
  }
}

}  // namespace media
