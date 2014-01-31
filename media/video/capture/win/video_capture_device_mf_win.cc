// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/video_capture_device_mf_win.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/video/capture/win/capability_list_win.h"

using base::win::ScopedCoMem;
using base::win::ScopedComPtr;

namespace media {
namespace {

// In Windows device identifiers, the USB VID and PID are preceded by the string
// "vid_" or "pid_".  The identifiers are each 4 bytes long.
const char kVidPrefix[] = "vid_";  // Also contains '\0'.
const char kPidPrefix[] = "pid_";  // Also contains '\0'.
const size_t kVidPidSize = 4;

class MFInitializerSingleton {
 public:
  MFInitializerSingleton() { MFStartup(MF_VERSION, MFSTARTUP_LITE); }
  ~MFInitializerSingleton() { MFShutdown(); }
};

static base::LazyInstance<MFInitializerSingleton> g_mf_initialize =
    LAZY_INSTANCE_INITIALIZER;

void EnsureMFInit() {
  g_mf_initialize.Get();
}

bool PrepareVideoCaptureAttributes(IMFAttributes** attributes, int count) {
  EnsureMFInit();

  if (FAILED(MFCreateAttributes(attributes, count)))
    return false;

  return SUCCEEDED((*attributes)->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
      MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
}

bool EnumerateVideoDevices(IMFActivate*** devices,
                           UINT32* count) {
  ScopedComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributes(attributes.Receive(), 1))
    return false;

  return SUCCEEDED(MFEnumDeviceSources(attributes, devices, count));
}

bool CreateVideoCaptureDevice(const char* sym_link, IMFMediaSource** source) {
  ScopedComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributes(attributes.Receive(), 2))
    return false;

  attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                        base::SysUTF8ToWide(sym_link).c_str());

  return SUCCEEDED(MFCreateDeviceSource(attributes, source));
}

bool FormatFromGuid(const GUID& guid, VideoPixelFormat* format) {
  struct {
    const GUID& guid;
    const VideoPixelFormat format;
  } static const kFormatMap[] = {
    { MFVideoFormat_I420, PIXEL_FORMAT_I420 },
    { MFVideoFormat_YUY2, PIXEL_FORMAT_YUY2 },
    { MFVideoFormat_UYVY, PIXEL_FORMAT_UYVY },
    { MFVideoFormat_RGB24, PIXEL_FORMAT_RGB24 },
    { MFVideoFormat_ARGB32, PIXEL_FORMAT_ARGB },
    { MFVideoFormat_MJPG, PIXEL_FORMAT_MJPEG },
    { MFVideoFormat_YV12, PIXEL_FORMAT_YV12 },
  };

  for (int i = 0; i < arraysize(kFormatMap); ++i) {
    if (kFormatMap[i].guid == guid) {
      *format = kFormatMap[i].format;
      return true;
    }
  }

  return false;
}

bool GetFrameSize(IMFMediaType* type, gfx::Size* frame_size) {
  UINT32 width32, height32;
  if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width32, &height32)))
    return false;
  frame_size->SetSize(width32, height32);
  return true;
}

bool GetFrameRate(IMFMediaType* type,
                  int* frame_rate_numerator,
                  int* frame_rate_denominator) {
  UINT32 numerator, denominator;
  if (FAILED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator,
                                 &denominator))||
      !denominator) {
    return false;
  }
  *frame_rate_numerator = numerator;
  *frame_rate_denominator = denominator;
  return true;
}

bool FillCapabilitiesFromType(IMFMediaType* type,
                              VideoCaptureCapabilityWin* capability) {
  GUID type_guid;
  if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &type_guid)) ||
      !GetFrameSize(type, &capability->supported_format.frame_size) ||
      !GetFrameRate(type,
                    &capability->frame_rate_numerator,
                    &capability->frame_rate_denominator) ||
      !FormatFromGuid(type_guid, &capability->supported_format.pixel_format)) {
    return false;
  }
  // Keep the integer version of the frame_rate for (potential) returns.
  capability->supported_format.frame_rate =
      capability->frame_rate_numerator / capability->frame_rate_denominator;

  return true;
}

HRESULT FillCapabilities(IMFSourceReader* source,
                         CapabilityList* capabilities) {
  DWORD stream_index = 0;
  ScopedComPtr<IMFMediaType> type;
  HRESULT hr;
  while (SUCCEEDED(hr = source->GetNativeMediaType(
      MF_SOURCE_READER_FIRST_VIDEO_STREAM, stream_index, type.Receive()))) {
    VideoCaptureCapabilityWin capability(stream_index++);
    if (FillCapabilitiesFromType(type, &capability))
      capabilities->Add(capability);
    type.Release();
  }

  if (capabilities->empty() && (SUCCEEDED(hr) || hr == MF_E_NO_MORE_TYPES))
    hr = HRESULT_FROM_WIN32(ERROR_EMPTY);

  return (hr == MF_E_NO_MORE_TYPES) ? S_OK : hr;
}

bool LoadMediaFoundationDlls() {
  static const wchar_t* const kMfDLLs[] = {
    L"%WINDIR%\\system32\\mf.dll",
    L"%WINDIR%\\system32\\mfplat.dll",
    L"%WINDIR%\\system32\\mfreadwrite.dll",
  };

  for (int i = 0; i < arraysize(kMfDLLs); ++i) {
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(kMfDLLs[i], path, arraysize(path));
    if (!LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
      return false;
  }

  return true;
}

}  // namespace

class MFReaderCallback
    : public base::RefCountedThreadSafe<MFReaderCallback>,
      public IMFSourceReaderCallback {
 public:
  MFReaderCallback(VideoCaptureDeviceMFWin* observer)
      : observer_(observer), wait_event_(NULL) {
  }

  void SetSignalOnFlush(base::WaitableEvent* event) {
    wait_event_ = event;
  }

  STDMETHOD(QueryInterface)(REFIID riid, void** object) {
    if (riid != IID_IUnknown && riid != IID_IMFSourceReaderCallback)
      return E_NOINTERFACE;
    *object = static_cast<IMFSourceReaderCallback*>(this);
    AddRef();
    return S_OK;
  }

  STDMETHOD_(ULONG, AddRef)() {
    base::RefCountedThreadSafe<MFReaderCallback>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() {
    base::RefCountedThreadSafe<MFReaderCallback>::Release();
    return 1U;
  }

  STDMETHOD(OnReadSample)(HRESULT status, DWORD stream_index,
      DWORD stream_flags, LONGLONG time_stamp, IMFSample* sample) {
    base::TimeTicks stamp(base::TimeTicks::Now());
    if (!sample) {
      observer_->OnIncomingCapturedFrame(NULL, 0, stamp, 0);
      return S_OK;
    }

    DWORD count = 0;
    sample->GetBufferCount(&count);

    for (DWORD i = 0; i < count; ++i) {
      ScopedComPtr<IMFMediaBuffer> buffer;
      sample->GetBufferByIndex(i, buffer.Receive());
      if (buffer) {
        DWORD length = 0, max_length = 0;
        BYTE* data = NULL;
        buffer->Lock(&data, &max_length, &length);
        observer_->OnIncomingCapturedFrame(data, length, stamp, 0);
        buffer->Unlock();
      }
    }
    return S_OK;
  }

  STDMETHOD(OnFlush)(DWORD stream_index) {
    if (wait_event_) {
      wait_event_->Signal();
      wait_event_ = NULL;
    }
    return S_OK;
  }

  STDMETHOD(OnEvent)(DWORD stream_index, IMFMediaEvent* event) {
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
bool VideoCaptureDeviceMFWin::PlatformSupported() {
  // Even though the DLLs might be available on Vista, we get crashes
  // when running our tests on the build bots.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  static bool g_dlls_available = LoadMediaFoundationDlls();
  return g_dlls_available;
}

// static
void VideoCaptureDeviceMFWin::GetDeviceNames(Names* device_names) {
  ScopedCoMem<IMFActivate*> devices;
  UINT32 count;
  if (!EnumerateVideoDevices(&devices, &count))
    return;

  HRESULT hr;
  for (UINT32 i = 0; i < count; ++i) {
    UINT32 name_size, id_size;
    ScopedCoMem<wchar_t> name, id;
    if (SUCCEEDED(hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &name_size)) &&
        SUCCEEDED(hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id,
            &id_size))) {
      std::wstring name_w(name, name_size), id_w(id, id_size);
      Name device(base::SysWideToUTF8(name_w), base::SysWideToUTF8(id_w),
          Name::MEDIA_FOUNDATION);
      device_names->push_back(device);
    } else {
      DLOG(WARNING) << "GetAllocatedString failed: " << std::hex << hr;
    }
    devices[i]->Release();
  }
}

// static
void VideoCaptureDeviceMFWin::GetDeviceSupportedFormats(const Name& device,
    VideoCaptureFormats* formats) {
  ScopedComPtr<IMFMediaSource> source;
  if (!CreateVideoCaptureDevice(device.id().c_str(), source.Receive()))
    return;

  HRESULT hr;
  base::win::ScopedComPtr<IMFSourceReader> reader;
  if (FAILED(hr = MFCreateSourceReaderFromMediaSource(source, NULL,
                                                      reader.Receive()))) {
    DLOG(ERROR) << "MFCreateSourceReaderFromMediaSource: " << std::hex << hr;
    return;
  }

  DWORD stream_index = 0;
  ScopedComPtr<IMFMediaType> type;
  while (SUCCEEDED(hr = reader->GetNativeMediaType(
         MF_SOURCE_READER_FIRST_VIDEO_STREAM, stream_index, type.Receive()))) {
    UINT32 width, height;
    hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize: " << std::hex << hr;
      return;
    }
    VideoCaptureFormat capture_format;
    capture_format.frame_size.SetSize(width, height);

    UINT32 numerator, denominator;
    hr = MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator, &denominator);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize: " << std::hex << hr;
      return;
    }
    capture_format.frame_rate = denominator ? numerator / denominator : 0;

    GUID type_guid;
    hr = type->GetGUID(MF_MT_SUBTYPE, &type_guid);
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetGUID: " << std::hex << hr;
      return;
    }
    FormatFromGuid(type_guid, &capture_format.pixel_format);
    type.Release();
    formats->push_back(capture_format);
    ++stream_index;

    DVLOG(1) << device.name() << " resolution: "
             << capture_format.frame_size.ToString() << ", fps: "
             << capture_format.frame_rate << ", pixel format: "
             << capture_format.pixel_format;
  }
}

const std::string VideoCaptureDevice::Name::GetModel() const {
  const size_t vid_prefix_size = sizeof(kVidPrefix) - 1;
  const size_t pid_prefix_size = sizeof(kPidPrefix) - 1;
  const size_t vid_location = unique_id_.find(kVidPrefix);
  if (vid_location == std::string::npos ||
      vid_location + vid_prefix_size + kVidPidSize > unique_id_.size()) {
    return "";
  }
  const size_t pid_location = unique_id_.find(kPidPrefix);
  if (pid_location == std::string::npos ||
      pid_location + pid_prefix_size + kVidPidSize > unique_id_.size()) {
    return "";
  }
  std::string id_vendor =
      unique_id_.substr(vid_location + vid_prefix_size, kVidPidSize);
  std::string id_product =
      unique_id_.substr(pid_location + pid_prefix_size, kVidPidSize);
  return id_vendor + ":" + id_product;
}

VideoCaptureDeviceMFWin::VideoCaptureDeviceMFWin(const Name& device_name)
    : name_(device_name), capture_(0) {
  DetachFromThread();
}

VideoCaptureDeviceMFWin::~VideoCaptureDeviceMFWin() {
  DCHECK(CalledOnValidThread());
}

bool VideoCaptureDeviceMFWin::Init() {
  DCHECK(CalledOnValidThread());
  DCHECK(!reader_);

  ScopedComPtr<IMFMediaSource> source;
  if (!CreateVideoCaptureDevice(name_.id().c_str(), source.Receive()))
    return false;

  ScopedComPtr<IMFAttributes> attributes;
  MFCreateAttributes(attributes.Receive(), 1);
  DCHECK(attributes);

  callback_ = new MFReaderCallback(this);
  attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback_.get());

  return SUCCEEDED(MFCreateSourceReaderFromMediaSource(source, attributes,
                                                       reader_.Receive()));
}

void VideoCaptureDeviceMFWin::AllocateAndStart(
    const VideoCaptureParams& params,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(CalledOnValidThread());

  base::AutoLock lock(lock_);

  client_ = client.Pass();
  DCHECK_EQ(capture_, false);

  CapabilityList capabilities;
  HRESULT hr = S_OK;
  if (!reader_ || FAILED(hr = FillCapabilities(reader_, &capabilities))) {
    OnError(hr);
    return;
  }

  VideoCaptureCapabilityWin found_capability =
      capabilities.GetBestMatchedFormat(
          params.requested_format.frame_size.width(),
          params.requested_format.frame_size.height(),
          params.requested_format.frame_rate);

  ScopedComPtr<IMFMediaType> type;
  if (FAILED(hr = reader_->GetNativeMediaType(
          MF_SOURCE_READER_FIRST_VIDEO_STREAM, found_capability.stream_index,
          type.Receive())) ||
      FAILED(hr = reader_->SetCurrentMediaType(
          MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type))) {
    OnError(hr);
    return;
  }

  if (FAILED(hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                      NULL, NULL, NULL, NULL))) {
    OnError(hr);
    return;
  }
  capture_format_ = found_capability.supported_format;
  capture_ = true;
}

void VideoCaptureDeviceMFWin::StopAndDeAllocate() {
  DCHECK(CalledOnValidThread());
  base::WaitableEvent flushed(false, false);
  const int kFlushTimeOutInMs = 1000;
  bool wait = false;
  {
    base::AutoLock lock(lock_);
    if (capture_) {
      capture_ = false;
      callback_->SetSignalOnFlush(&flushed);
      HRESULT hr = reader_->Flush(MF_SOURCE_READER_ALL_STREAMS);
      wait = SUCCEEDED(hr);
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

void VideoCaptureDeviceMFWin::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    const base::TimeTicks& time_stamp,
    int rotation) {
  base::AutoLock lock(lock_);
  if (data && client_.get())
    client_->OnIncomingCapturedFrame(data,
                                     length,
                                     time_stamp,
                                     rotation,
                                     capture_format_);

  if (capture_) {
    HRESULT hr = reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                                     NULL, NULL, NULL, NULL);
    if (FAILED(hr)) {
      // If running the *VideoCap* unit tests on repeat, this can sometimes
      // fail with HRESULT_FROM_WINHRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION).
      // It's not clear to me why this is, but it is possible that it has
      // something to do with this bug:
      // http://support.microsoft.com/kb/979567
      OnError(hr);
    }
  }
}

void VideoCaptureDeviceMFWin::OnError(HRESULT hr) {
  std::string log_msg = base::StringPrintf("VideoCaptureDeviceMFWin: %x", hr);
  DLOG(ERROR) << log_msg;
  if (client_.get())
    client_->OnError(log_msg);
}

}  // namespace media
