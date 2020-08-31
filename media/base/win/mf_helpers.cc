// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_helpers.h"

namespace media {

Microsoft::WRL::ComPtr<IMFSample> CreateEmptySampleWithBuffer(
    uint32_t buffer_length,
    int align) {
  CHECK_GT(buffer_length, 0U);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(&sample);
  RETURN_ON_HR_FAILURE(hr, "MFCreateSample failed",
                       Microsoft::WRL::ComPtr<IMFSample>());

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  if (align == 0) {
    // Note that MFCreateMemoryBuffer is same as MFCreateAlignedMemoryBuffer
    // with the align argument being 0.
    hr = MFCreateMemoryBuffer(buffer_length, &buffer);
  } else {
    hr = MFCreateAlignedMemoryBuffer(buffer_length, align - 1, &buffer);
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for sample",
                       Microsoft::WRL::ComPtr<IMFSample>());

  hr = sample->AddBuffer(buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample",
                       Microsoft::WRL::ComPtr<IMFSample>());

  buffer->SetCurrentLength(0);
  return sample;
}

MediaBufferScopedPointer::MediaBufferScopedPointer(IMFMediaBuffer* media_buffer)
    : media_buffer_(media_buffer),
      buffer_(nullptr),
      max_length_(0),
      current_length_(0) {
  HRESULT hr = media_buffer_->Lock(&buffer_, &max_length_, &current_length_);
  CHECK(SUCCEEDED(hr));
}

MediaBufferScopedPointer::~MediaBufferScopedPointer() {
  HRESULT hr = media_buffer_->Unlock();
  CHECK(SUCCEEDED(hr));
}

DXGIDeviceScopedHandle::DXGIDeviceScopedHandle(
    IMFDXGIDeviceManager* device_manager)
    : device_manager_(device_manager) {}

DXGIDeviceScopedHandle::~DXGIDeviceScopedHandle() {
  if (device_handle_ != INVALID_HANDLE_VALUE) {
    HRESULT hr = device_manager_->CloseDeviceHandle(device_handle_);
    CHECK(SUCCEEDED(hr));
    device_handle_ = INVALID_HANDLE_VALUE;
  }
}

HRESULT DXGIDeviceScopedHandle::LockDevice(REFIID riid, void** device_out) {
  HRESULT hr;
  if (device_handle_ == INVALID_HANDLE_VALUE) {
    hr = device_manager_->OpenDeviceHandle(&device_handle_);
    if (FAILED(hr)) {
      return hr;
    }
  }
  // see
  // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgidevicemanager-lockdevice
  // for details of LockDevice call.
  hr = device_manager_->LockDevice(device_handle_, riid, device_out,
                                   /*block=*/FALSE);
  return hr;
}

}  // namespace media
