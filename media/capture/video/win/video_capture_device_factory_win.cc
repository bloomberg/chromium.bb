// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_factory_win.h"

#include <mfapi.h>
#include <mferror.h>
#include <objbase.h>
#include <stddef.h>
#include <wrl/client.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_initializer.h"
#include "media/capture/video/win/metrics.h"
#include "media/capture/video/win/video_capture_device_mf_win.h"
#include "media/capture/video/win/video_capture_device_win.h"

using Descriptor = media::VideoCaptureDeviceDescriptor;
using Descriptors = media::VideoCaptureDeviceDescriptors;
using base::win::ScopedCoMem;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace media {

namespace {

// In Windows device identifiers, the USB VID and PID are preceded by the string
// "vid_" or "pid_".  The identifiers are each 4 bytes long.
const char kVidPrefix[] = "vid_";  // Also contains '\0'.
const char kPidPrefix[] = "pid_";  // Also contains '\0'.
const size_t kVidPidSize = 4;

// Avoid enumerating and/or using certain devices due to they provoking crashes
// or any other reason (http://crbug.com/378494). This enum is defined for the
// purposes of UMA collection. Existing entries cannot be removed.
enum BlacklistedCameraNames {
  BLACKLISTED_CAMERA_GOOGLE_CAMERA_ADAPTER = 0,
  BLACKLISTED_CAMERA_IP_CAMERA = 1,
  BLACKLISTED_CAMERA_CYBERLINK_WEBCAM_SPLITTER = 2,
  BLACKLISTED_CAMERA_EPOCCAM = 3,
  // This one must be last, and equal to the previous enumerated value.
  BLACKLISTED_CAMERA_MAX = BLACKLISTED_CAMERA_EPOCCAM,
};

// Blacklisted devices are identified by a characteristic prefix of the name.
// This prefix is used case-insensitively. This list must be kept in sync with
// |BlacklistedCameraNames|.
const char* const kBlacklistedCameraNames[] = {
    // Name of a fake DirectShow filter on computers with GTalk installed.
    "Google Camera Adapter",
    // The following software WebCams cause crashes.
    "IP Camera [JPEG/MJPEG]", "CyberLink Webcam Splitter", "EpocCam",
};
static_assert(arraysize(kBlacklistedCameraNames) == BLACKLISTED_CAMERA_MAX + 1,
              "kBlacklistedCameraNames should be same size as "
              "BlacklistedCameraNames enum");

const std::pair<VideoCaptureApi, std::vector<std::pair<GUID, GUID>>>
    kMfAttributes[] = {{VideoCaptureApi::WIN_MEDIA_FOUNDATION,
                        {
                            {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                             MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                        }},
                       {VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR,
                        {{MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                          MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                         {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY,
                          KSCATEGORY_SENSOR_CAMERA}}}};

bool IsDeviceBlacklistedForQueryingDetailedFrameRates(
    const std::string& display_name) {
  return display_name.find("WebcamMax") != std::string::npos;
}

bool LoadMediaFoundationDlls() {
  static const wchar_t* const kMfDLLs[] = {
      L"%WINDIR%\\system32\\mf.dll", L"%WINDIR%\\system32\\mfplat.dll",
      L"%WINDIR%\\system32\\mfreadwrite.dll",
      L"%WINDIR%\\system32\\MFCaptureEngine.dll"};

  for (const wchar_t* kMfDLL : kMfDLLs) {
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(kMfDLL, path, arraysize(path));
    if (!LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
      return false;
  }
  return true;
}

bool PrepareVideoCaptureAttributesMediaFoundation(
    const std::vector<std::pair<GUID, GUID>>& attributes_data,
    int count,
    IMFAttributes** attributes) {
  // Once https://bugs.chromium.org/p/chromium/issues/detail?id=791615 is fixed,
  // we must make sure that this method succeeds in capture_unittests context
  // when MediaFoundation is enabled.
  if (!VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation() ||
      !InitializeMediaFoundation()) {
    return false;
  }

  if (FAILED(MFCreateAttributes(attributes, count)))
    return false;

  for (const auto& value : attributes_data) {
    if (!SUCCEEDED((*attributes)->SetGUID(value.first, value.second)))
      return false;
  }
  return true;
}

bool CreateVideoCaptureDeviceMediaFoundation(const Descriptor& descriptor,
                                             IMFMediaSource** source) {
  ComPtr<IMFAttributes> attributes;
  static_assert(
      arraysize(kMfAttributes) == 2,
      "Implementation here asumes that kMfAttributes has size of two.");
  DCHECK_EQ(kMfAttributes[0].first, VideoCaptureApi::WIN_MEDIA_FOUNDATION);
  const auto& attributes_data =
      descriptor.capture_api == VideoCaptureApi::WIN_MEDIA_FOUNDATION
          ? kMfAttributes[0].second
          : kMfAttributes[1].second;
  // We allocate attributes_data.size() + 1 (+1 is for sym_link below) elements
  // in attributes store.
  if (!PrepareVideoCaptureAttributesMediaFoundation(
          attributes_data, attributes_data.size() + 1,
          attributes.GetAddressOf())) {
    return false;
  }

  attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                        base::SysUTF8ToWide(descriptor.device_id).c_str());
  return SUCCEEDED(MFCreateDeviceSource(attributes.Get(), source));
}

bool EnumerateVideoDevicesMediaFoundation(
    const std::vector<std::pair<GUID, GUID>>& attributes_data,
    VideoCaptureDeviceFactoryWin::MFEnumDeviceSourcesFunc
        mf_enum_device_sources_func,
    IMFActivate*** devices,
    UINT32* count) {
  ComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributesMediaFoundation(
          attributes_data, attributes_data.size(), attributes.GetAddressOf())) {
    return false;
  }
  return SUCCEEDED(
      mf_enum_device_sources_func(attributes.Get(), devices, count));
}

bool IsDeviceBlackListed(const std::string& name) {
  DCHECK_EQ(BLACKLISTED_CAMERA_MAX + 1,
            static_cast<int>(arraysize(kBlacklistedCameraNames)));
  for (size_t i = 0; i < arraysize(kBlacklistedCameraNames); ++i) {
    if (base::StartsWith(name, kBlacklistedCameraNames[i],
                         base::CompareCase::INSENSITIVE_ASCII)) {
      DVLOG(1) << "Enumerated blacklisted device: " << name;
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.BlacklistedDevice", i,
                                BLACKLISTED_CAMERA_MAX + 1);
      return true;
    }
  }
  return false;
}

std::string GetDeviceModelId(const std::string& device_id) {
  const size_t vid_prefix_size = sizeof(kVidPrefix) - 1;
  const size_t pid_prefix_size = sizeof(kPidPrefix) - 1;
  const size_t vid_location = device_id.find(kVidPrefix);
  if (vid_location == std::string::npos ||
      vid_location + vid_prefix_size + kVidPidSize > device_id.size()) {
    return std::string();
  }
  const size_t pid_location = device_id.find(kPidPrefix);
  if (pid_location == std::string::npos ||
      pid_location + pid_prefix_size + kVidPidSize > device_id.size()) {
    return std::string();
  }
  const std::string id_vendor =
      device_id.substr(vid_location + vid_prefix_size, kVidPidSize);
  const std::string id_product =
      device_id.substr(pid_location + pid_prefix_size, kVidPidSize);
  return id_vendor + ":" + id_product;
}

void GetDeviceDescriptorsDirectShow(Descriptors* device_descriptors) {
  DCHECK(device_descriptors);
  DVLOG(1) << __func__;

  ComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                                  IID_PPV_ARGS(&dev_enum));
  if (FAILED(hr))
    return;

  ComPtr<IEnumMoniker> enum_moniker;
  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker.GetAddressOf(), 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return;

  // Enumerate all video capture devices.
  for (ComPtr<IMoniker> moniker;
       enum_moniker->Next(1, moniker.GetAddressOf(), NULL) == S_OK;
       moniker.Reset()) {
    ComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_PPV_ARGS(&prop_bag));
    if (FAILED(hr))
      continue;

    // Find the description or friendly name.
    ScopedVariant name;
    hr = prop_bag->Read(L"Description", name.Receive(), 0);
    if (FAILED(hr))
      hr = prop_bag->Read(L"FriendlyName", name.Receive(), 0);

    if (FAILED(hr) || name.type() != VT_BSTR)
      continue;

    const std::string device_name(base::SysWideToUTF8(V_BSTR(name.ptr())));
    if (IsDeviceBlackListed(device_name))
      continue;

    name.Reset();
    hr = prop_bag->Read(L"DevicePath", name.Receive(), 0);
    std::string id;
    if (FAILED(hr) || name.type() != VT_BSTR) {
      id = device_name;
    } else {
      DCHECK_EQ(name.type(), VT_BSTR);
      id = base::SysWideToUTF8(V_BSTR(name.ptr()));
    }

    const std::string model_id = GetDeviceModelId(id);

    device_descriptors->emplace_back(device_name, id, model_id,
                                     VideoCaptureApi::WIN_DIRECT_SHOW);
  }
}

bool DescriptorsContainDeviceId(const Descriptors& descriptors,
                                const std::string& device_id) {
  return std::find_if(
             descriptors.begin(), descriptors.end(),
             [device_id](const VideoCaptureDeviceDescriptor& descriptor) {
               return device_id == descriptor.device_id;
             }) != descriptors.end();
}

void GetDeviceDescriptorsMediaFoundation(
    VideoCaptureDeviceFactoryWin::MFEnumDeviceSourcesFunc
        mf_enum_device_sources_func,
    Descriptors* device_descriptors) {
  DVLOG(1) << " GetDeviceDescriptorsMediaFoundation";
  // Recent non-RGB (depth, IR) cameras could be marked as sensor cameras in
  // driver inf file and MFEnumDeviceSources enumerates them only if attribute
  // KSCATEGORY_SENSOR_CAMERA is supplied. We enumerate twice. As it is possible
  // that SENSOR_CAMERA is also in VIDEO_CAMERA category, we prevent duplicate
  // entries. https://crbug.com/807293
  for (const auto& api_attributes : kMfAttributes) {
    ScopedCoMem<IMFActivate*> devices;
    UINT32 count;
    if (!EnumerateVideoDevicesMediaFoundation(api_attributes.second,
                                              mf_enum_device_sources_func,
                                              &devices, &count)) {
      return;
    }
    const bool list_was_empty = !device_descriptors->size();
    for (UINT32 i = 0; i < count; ++i) {
      ScopedCoMem<wchar_t> name;
      UINT32 name_size;
      HRESULT hr = devices[i]->GetAllocatedString(
          MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &name_size);
      if (SUCCEEDED(hr)) {
        ScopedCoMem<wchar_t> id;
        UINT32 id_size;
        hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id,
            &id_size);
        if (SUCCEEDED(hr)) {
          const std::string device_id =
              base::SysWideToUTF8(std::wstring(id, id_size));
          const std::string model_id = GetDeviceModelId(device_id);
          if (list_was_empty ||
              !DescriptorsContainDeviceId(*device_descriptors, device_id)) {
            device_descriptors->emplace_back(
                base::SysWideToUTF8(std::wstring(name, name_size)), device_id,
                model_id, api_attributes.first);
          }
        }
      }
      DLOG_IF(ERROR, FAILED(hr)) << "GetAllocatedString failed: "
                                 << logging::SystemErrorCodeToString(hr);
      devices[i]->Release();
    }
  }
}

void GetDeviceSupportedFormatsDirectShow(const Descriptor& descriptor,
                                         VideoCaptureFormats* formats) {
  DVLOG(1) << "GetDeviceSupportedFormatsDirectShow for "
           << descriptor.display_name();
  bool query_detailed_frame_rates =
      !IsDeviceBlacklistedForQueryingDetailedFrameRates(
          descriptor.display_name());
  CapabilityList capability_list;
  VideoCaptureDeviceWin::GetDeviceCapabilityList(
      descriptor.device_id, query_detailed_frame_rates, &capability_list);
  for (const auto& entry : capability_list) {
    formats->emplace_back(entry.supported_format);
    DVLOG(1) << descriptor.display_name() << " "
             << VideoCaptureFormat::ToString(entry.supported_format);
  }
}

void GetDeviceSupportedFormatsMediaFoundation(const Descriptor& descriptor,
                                              VideoCaptureFormats* formats) {
  DVLOG(1) << "GetDeviceSupportedFormatsMediaFoundation for "
           << descriptor.display_name();
  ComPtr<IMFMediaSource> source;
  if (!CreateVideoCaptureDeviceMediaFoundation(descriptor,
                                               source.GetAddressOf())) {
    return;
  }

  ComPtr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromMediaSource(source.Get(), NULL,
                                                   reader.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "MFCreateSourceReaderFromMediaSource failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  DWORD stream_index = 0;
  ComPtr<IMFMediaType> type;
  while (SUCCEEDED(hr = reader->GetNativeMediaType(
                       static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                       stream_index, type.GetAddressOf()))) {
    UINT32 width, height;
    hr = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return;
    }
    VideoCaptureFormat capture_format;
    capture_format.frame_size.SetSize(width, height);

    UINT32 numerator, denominator;
    hr = MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &numerator,
                             &denominator);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return;
    }
    capture_format.frame_rate =
        denominator ? static_cast<float>(numerator) / denominator : 0.0f;

    GUID type_guid;
    hr = type->GetGUID(MF_MT_SUBTYPE, &type_guid);
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetGUID failed: " << logging::SystemErrorCodeToString(hr);
      return;
    }
    VideoCaptureDeviceMFWin::GetPixelFormatFromMFSourceMediaSubtype(
        type_guid, &capture_format.pixel_format);
    type.Reset();
    ++stream_index;
    if (capture_format.pixel_format == PIXEL_FORMAT_UNKNOWN)
      continue;
    formats->push_back(capture_format);

    DVLOG(1) << descriptor.display_name() << " "
             << VideoCaptureFormat::ToString(capture_format);
  }
}

}  // namespace

// Returns true iff the current platform supports the Media Foundation API
// and that the DLLs are available.  On Vista this API is an optional download
// but the API is advertised as a part of Windows 7 and onwards.  However,
// we've seen that the required DLLs are not available in some Win7
// distributions such as Windows 7 N and Windows 7 KN.
// static
bool VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation() {
  static bool g_dlls_available = LoadMediaFoundationDlls();
  return g_dlls_available;
}

VideoCaptureDeviceFactoryWin::VideoCaptureDeviceFactoryWin()
    : use_media_foundation_(
          base::FeatureList::IsEnabled(media::kMediaFoundationVideoCapture)) {
  mf_enum_device_sources_func_ =
      PlatformSupportsMediaFoundation() ? MFEnumDeviceSources : nullptr;
  if (!PlatformSupportsMediaFoundation()) {
    use_media_foundation_ = false;
    LogVideoCaptureWinBackendUsed(
        VideoCaptureWinBackendUsed::kUsingDirectShowAsFallback);
  } else if (use_media_foundation_) {
    LogVideoCaptureWinBackendUsed(
        VideoCaptureWinBackendUsed::kUsingMediaFoundationAsDefault);
  } else {
    LogVideoCaptureWinBackendUsed(
        VideoCaptureWinBackendUsed::kUsingDirectShowAsDefault);
  }
}

std::unique_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryWin::CreateDevice(
    const Descriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (device_descriptor.capture_api) {
    case VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      FALLTHROUGH;
    case VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR: {
      DCHECK(PlatformSupportsMediaFoundation());
      ComPtr<IMFMediaSource> source;
      if (!CreateVideoCaptureDeviceMediaFoundation(device_descriptor,
                                                   source.GetAddressOf())) {
        break;
      }
      std::unique_ptr<VideoCaptureDevice> device(
          new VideoCaptureDeviceMFWin(source));
      DVLOG(1) << " MediaFoundation Device: "
               << device_descriptor.display_name();
      if (static_cast<VideoCaptureDeviceMFWin*>(device.get())->Init())
        return device;
      break;
    }
    case VideoCaptureApi::WIN_DIRECT_SHOW: {
      DVLOG(1) << " DirectShow Device: " << device_descriptor.display_name();
      std::unique_ptr<VideoCaptureDevice> device(
          new VideoCaptureDeviceWin(device_descriptor));
      if (static_cast<VideoCaptureDeviceWin*>(device.get())->Init())
        return device;
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void VideoCaptureDeviceFactoryWin::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_media_foundation_) {
    GetDeviceDescriptorsMediaFoundation(mf_enum_device_sources_func_,
                                        device_descriptors);
  } else {
    GetDeviceDescriptorsDirectShow(device_descriptors);
  }
}

void VideoCaptureDeviceFactoryWin::GetSupportedFormats(
    const Descriptor& device,
    VideoCaptureFormats* formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_media_foundation_)
    GetDeviceSupportedFormatsMediaFoundation(device, formats);
  else
    GetDeviceSupportedFormatsDirectShow(device, formats);
}

// static
VideoCaptureDeviceFactory*
VideoCaptureDeviceFactory::CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    MojoJpegDecodeAcceleratorFactoryCB jda_factory,
    MojoJpegEncodeAcceleratorFactoryCB jea_factory) {
  return new VideoCaptureDeviceFactoryWin();
}

}  // namespace media
