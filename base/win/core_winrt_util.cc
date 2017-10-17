// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/core_winrt_util.h"

#include <roapi.h>
#include <robuffer.h>
#include <string.h>
#include <wrl/client.h>

#include "base/strings/string_util.h"
#include "base/win/scoped_hstring.h"

namespace {

void* LoadComBaseFunction(const char* function_name) {
  static HMODULE const handle = ::LoadLibrary(L"combase.dll");
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::RoActivateInstance) GetRoActivateInstanceFunction() {
  static decltype(&::RoActivateInstance) const function =
      reinterpret_cast<decltype(&::RoActivateInstance)>(
          LoadComBaseFunction("RoActivateInstance"));
  return function;
}

decltype(&::RoGetActivationFactory) GetRoGetActivationFactoryFunction() {
  static decltype(&::RoGetActivationFactory) const function =
      reinterpret_cast<decltype(&::RoGetActivationFactory)>(
          LoadComBaseFunction("RoGetActivationFactory"));
  return function;
}

}  // namespace

namespace base {
namespace win {

bool ResolveCoreWinRTDelayload() {
  // TODO(finnur): Add AssertIOAllowed once crbug.com/770193 is fixed.

  return GetRoActivateInstanceFunction() && GetRoGetActivationFactoryFunction();
}

HRESULT RoGetActivationFactory(HSTRING class_id,
                               const IID& iid,
                               void** out_factory) {
  decltype(&::RoGetActivationFactory) get_factory_func =
      GetRoGetActivationFactoryFunction();
  if (!get_factory_func)
    return E_FAIL;
  return get_factory_func(class_id, iid, out_factory);
}

HRESULT RoActivateInstance(HSTRING class_id, IInspectable** instance) {
  decltype(&::RoActivateInstance) activate_instance_func =
      GetRoActivateInstanceFunction();
  if (!activate_instance_func)
    return E_FAIL;
  return activate_instance_func(class_id, instance);
}

HRESULT GetPointerToBufferData(ABI::Windows::Storage::Streams::IBuffer* buffer,
                               uint8_t** out) {
  *out = nullptr;

  Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess>
      buffer_byte_access;
  HRESULT hr = buffer->QueryInterface(IID_PPV_ARGS(&buffer_byte_access));
  if (FAILED(hr))
    return hr;

  // Lifetime of the pointing buffer is controlled by the buffer object.
  return buffer_byte_access->Buffer(out);
}

HRESULT CreateIBufferFromData(
    const uint8_t* data,
    UINT32 length,
    ABI::Windows::Storage::Streams::IBuffer** buffer) {
  Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBufferFactory>
      buffer_factory;
  HRESULT hr = base::win::GetActivationFactory<
      ABI::Windows::Storage::Streams::IBufferFactory,
      RuntimeClass_Windows_Storage_Streams_Buffer>(&buffer_factory);
  if (FAILED(hr))
    return hr;

  hr = buffer_factory->Create(length, buffer);
  if (FAILED(hr))
    return hr;

  hr = (*buffer)->put_Length(length);
  if (FAILED(hr))
    return hr;

  uint8_t* p_buffer_data;
  hr = GetPointerToBufferData(*buffer, &p_buffer_data);
  if (FAILED(hr))
    return hr;

  memcpy(p_buffer_data, data, length);

  return S_OK;
}

}  // namespace win
}  // namespace base
