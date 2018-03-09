// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfidl.h>

#include <ks.h>
#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>

#include "base/strings/sys_string_conversions.h"
#include "media/capture/video/win/video_capture_device_factory_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace media {

namespace {

const wchar_t* kDeviceId0 = L"\\\\?\\usb#vid_0000&pid_0000&mi_00";
const wchar_t* kDeviceId1 = L"\\\\?\\usb#vid_0001&pid_0001&mi_00";
const wchar_t* kDeviceId2 = L"\\\\?\\usb#vid_0002&pid_0002&mi_00";
const wchar_t* kDeviceName0 = L"Device 0";
const wchar_t* kDeviceName1 = L"Device 1";
const wchar_t* kDeviceName2 = L"Device 2";

using iterator = VideoCaptureDeviceDescriptors::const_iterator;
iterator FindDescriptorInRange(iterator begin,
                               iterator end,
                               const std::string& device_id) {
  return std::find_if(
      begin, end, [device_id](const VideoCaptureDeviceDescriptor& descriptor) {
        return device_id == descriptor.device_id;
      });
}

class MockMFActivate : public base::RefCountedThreadSafe<MockMFActivate>,
                       public IMFActivate {
 public:
  MockMFActivate(const std::wstring& symbolic_link,
                 const std::wstring& name,
                 bool kscategory_video_camera,
                 bool kscategory_sensor_camera)
      : symbolic_link_(symbolic_link),
        name_(name),
        kscategory_video_camera_(kscategory_video_camera),
        kscategory_sensor_camera_(kscategory_sensor_camera) {}

  bool MatchesQuery(IMFAttributes* query, HRESULT* status) {
    UINT32 count;
    *status = query->GetCount(&count);
    if (FAILED(*status))
      return false;
    GUID value;
    *status = query->GetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &value);
    if (FAILED(*status))
      return false;
    if (value != MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)
      return false;
    *status = query->GetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY,
                             &value);
    if (SUCCEEDED(*status)) {
      if ((value == KSCATEGORY_SENSOR_CAMERA && kscategory_sensor_camera_) ||
          (value == KSCATEGORY_VIDEO_CAMERA && kscategory_video_camera_))
        return true;
    } else if (*status == MF_E_ATTRIBUTENOTFOUND) {
      // When no category attribute is specified, it should behave the same as
      // if KSCATEGORY_VIDEO_CAMERA is specified.
      *status = S_OK;
      if (kscategory_video_camera_)
        return true;
    }
    return false;
  }

  STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override {
    return E_NOTIMPL;
  }
  STDMETHOD_(ULONG, AddRef)() override {
    base::RefCountedThreadSafe<MockMFActivate>::AddRef();
    return 1U;
  }

  STDMETHOD_(ULONG, Release)() override {
    base::RefCountedThreadSafe<MockMFActivate>::Release();
    return 1U;
  }
  STDMETHOD(GetItem)(REFGUID key, PROPVARIANT* value) override {
    return E_FAIL;
  }
  STDMETHOD(GetItemType)(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType) override {
    return E_NOTIMPL;
  }
  STDMETHOD(CompareItem)
  (REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult) override {
    return E_NOTIMPL;
  }
  STDMETHOD(Compare)
  (IMFAttributes* pTheirs,
   MF_ATTRIBUTES_MATCH_TYPE MatchType,
   BOOL* pbResult) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetUINT32)(REFGUID key, UINT32* value) override {
    if (key == MF_MT_INTERLACE_MODE) {
      *value = MFVideoInterlace_Progressive;
      return S_OK;
    }
    return E_NOTIMPL;
  }
  STDMETHOD(GetUINT64)(REFGUID key, UINT64* value) override { return E_FAIL; }
  STDMETHOD(GetDouble)(REFGUID guidKey, double* pfValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetGUID)(REFGUID key, GUID* value) override { return E_FAIL; }
  STDMETHOD(GetStringLength)(REFGUID guidKey, UINT32* pcchLength) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetString)
  (REFGUID guidKey,
   LPWSTR pwszValue,
   UINT32 cchBufSize,
   UINT32* pcchLength) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetAllocatedString)
  (REFGUID guidKey, LPWSTR* ppwszValue, UINT32* pcchLength) override {
    std::wstring value;
    if (guidKey == MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK) {
      value = symbolic_link_;
    } else if (guidKey == MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME) {
      value = name_;
    } else {
      return E_NOTIMPL;
    }
    *ppwszValue = static_cast<wchar_t*>(
        CoTaskMemAlloc((value.size() + 1) * sizeof(wchar_t)));
    wcscpy(*ppwszValue, value.c_str());
    *pcchLength = value.length();
    return S_OK;
  }
  STDMETHOD(GetBlobSize)(REFGUID guidKey, UINT32* pcbBlobSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetBlob)
  (REFGUID guidKey,
   UINT8* pBuf,
   UINT32 cbBufSize,
   UINT32* pcbBlobSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetAllocatedBlob)
  (REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(GetUnknown)(REFGUID guidKey, REFIID riid, LPVOID* ppv) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetItem)(REFGUID guidKey, REFPROPVARIANT Value) override {
    return E_NOTIMPL;
  }
  STDMETHOD(DeleteItem)(REFGUID guidKey) override { return E_NOTIMPL; }
  STDMETHOD(DeleteAllItems)(void) override { return E_NOTIMPL; }
  STDMETHOD(SetUINT32)(REFGUID guidKey, UINT32 unValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetUINT64)(REFGUID guidKey, UINT64 unValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetDouble)(REFGUID guidKey, double fValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetGUID)(REFGUID guidKey, REFGUID guidValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetString)(REFGUID guidKey, LPCWSTR wszValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetBlob)
  (REFGUID guidKey, const UINT8* pBuf, UINT32 cbBufSize) override {
    return E_NOTIMPL;
  }
  STDMETHOD(SetUnknown)(REFGUID guidKey, IUnknown* pUnknown) override {
    return E_NOTIMPL;
  }
  STDMETHOD(LockStore)(void) override { return E_NOTIMPL; }
  STDMETHOD(UnlockStore)(void) override { return E_NOTIMPL; }
  STDMETHOD(GetCount)(UINT32* pcItems) override { return E_NOTIMPL; }
  STDMETHOD(GetItemByIndex)
  (UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue) override {
    return E_NOTIMPL;
  }
  STDMETHOD(CopyAllItems)(IMFAttributes* pDest) override { return E_NOTIMPL; }
  STDMETHOD(ActivateObject)(REFIID riid, void** ppv) override {
    return E_NOTIMPL;
  }
  STDMETHOD(DetachObject)(void) override { return E_NOTIMPL; }
  STDMETHOD(ShutdownObject)(void) override { return E_NOTIMPL; }

 private:
  friend class base::RefCountedThreadSafe<MockMFActivate>;
  virtual ~MockMFActivate() = default;

  const std::wstring symbolic_link_;
  const std::wstring name_;
  const bool kscategory_video_camera_;
  const bool kscategory_sensor_camera_;
};

HRESULT __stdcall MockMFEnumDeviceSources(IMFAttributes* attributes,
                                          IMFActivate*** devices,
                                          UINT32* count) {
  MockMFActivate* mock_devices[] = {
      new MockMFActivate(kDeviceId0, kDeviceName0, true, false),
      new MockMFActivate(kDeviceId1, kDeviceName1, true, true),
      new MockMFActivate(kDeviceId2, kDeviceName2, false, true)};
  // Iterate once to get the match count and check for errors.
  *count = 0U;
  HRESULT hr;
  for (MockMFActivate* device : mock_devices) {
    if (device->MatchesQuery(attributes, &hr))
      (*count)++;
    if (FAILED(hr))
      return hr;
  }
  // Second iteration packs the returned devices and increments their
  // reference count.
  *devices = static_cast<IMFActivate**>(
      CoTaskMemAlloc(sizeof(IMFActivate*) * (*count)));
  int offset = 0;
  for (MockMFActivate* device : mock_devices) {
    if (!device->MatchesQuery(attributes, &hr))
      continue;
    *(*devices + offset++) = device;
    device->AddRef();
  }
  return S_OK;
}

}  // namespace

class VideoCaptureDeviceFactoryWinTest : public ::testing::Test {
 protected:
  VideoCaptureDeviceFactoryWinTest()
      : media_foundation_supported_(
            VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()) {}

  bool ShouldSkipMFTest() {
    if (media_foundation_supported_)
      return false;
    DVLOG(1) << "Media foundation is not supported by the current platform. "
                "Skipping test.";
    return true;
  }

  VideoCaptureDeviceFactoryWin factory_;
  const bool media_foundation_supported_;
};

class VideoCaptureDeviceFactoryMFWinTest
    : public VideoCaptureDeviceFactoryWinTest {
  void SetUp() override { factory_.set_use_media_foundation_for_testing(true); }
};

TEST_F(VideoCaptureDeviceFactoryMFWinTest, GetDeviceDescriptors) {
  if (ShouldSkipMFTest())
    return;
  factory_.set_mf_enum_device_sources_func_for_testing(
      &MockMFEnumDeviceSources);
  VideoCaptureDeviceDescriptors descriptors;
  factory_.GetDeviceDescriptors(&descriptors);
  EXPECT_EQ(descriptors.size(), 3U);
  for (auto it = descriptors.begin(); it != descriptors.end(); it++) {
    // Verify that there are no duplicates.
    EXPECT_EQ(FindDescriptorInRange(descriptors.begin(), it, it->device_id),
              it);
  }
  iterator it = FindDescriptorInRange(descriptors.begin(), descriptors.end(),
                                      base::SysWideToUTF8(kDeviceId0));
  EXPECT_NE(it, descriptors.end());
  EXPECT_EQ(it->capture_api, VideoCaptureApi::WIN_MEDIA_FOUNDATION);
  EXPECT_EQ(it->display_name(), base::SysWideToUTF8(kDeviceName0));
  it = FindDescriptorInRange(descriptors.begin(), descriptors.end(),
                             base::SysWideToUTF8(kDeviceId1));
  EXPECT_NE(it, descriptors.end());
  EXPECT_EQ(it->capture_api, VideoCaptureApi::WIN_MEDIA_FOUNDATION);
  EXPECT_EQ(it->display_name(), base::SysWideToUTF8(kDeviceName1));
  it = FindDescriptorInRange(descriptors.begin(), descriptors.end(),
                             base::SysWideToUTF8(kDeviceId2));
  EXPECT_NE(it, descriptors.end());
  EXPECT_EQ(it->capture_api, VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR);
  EXPECT_EQ(it->display_name(), base::SysWideToUTF8(kDeviceName2));
}

}  // namespace media
