// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/wmi.h"

#include <windows.h>

#include <objbase.h>
#include <stdint.h>
#include <utility>

#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

using Microsoft::WRL::ComPtr;

namespace base {
namespace win {

bool CreateLocalWmiConnection(bool set_blanket,
                              ComPtr<IWbemServices>* wmi_services) {
  ComPtr<IWbemLocator> wmi_locator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&wmi_locator));
  if (FAILED(hr))
    return false;

  ComPtr<IWbemServices> wmi_services_r;
  hr = wmi_locator->ConnectServer(ScopedBstr(L"ROOT\\CIMV2"), nullptr, nullptr,
                                  nullptr, 0, nullptr, nullptr,
                                  wmi_services_r.GetAddressOf());
  if (FAILED(hr))
    return false;

  if (set_blanket) {
    hr = ::CoSetProxyBlanket(wmi_services_r.Get(), RPC_C_AUTHN_WINNT,
                             RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr))
      return false;
  }

  *wmi_services = std::move(wmi_services_r);
  return true;
}

bool CreateWmiClassMethodObject(IWbemServices* wmi_services,
                                const StringPiece16& class_name,
                                const StringPiece16& method_name,
                                ComPtr<IWbemClassObject>* class_instance) {
  // We attempt to instantiate a COM object that represents a WMI object plus
  // a method rolled into one entity.
  ScopedBstr b_class_name(class_name);
  ScopedBstr b_method_name(method_name);
  ComPtr<IWbemClassObject> class_object;
  HRESULT hr;
  hr = wmi_services->GetObject(b_class_name, 0, nullptr,
                               class_object.GetAddressOf(), nullptr);
  if (FAILED(hr))
    return false;

  ComPtr<IWbemClassObject> params_def;
  hr = class_object->GetMethod(b_method_name, 0, params_def.GetAddressOf(),
                               nullptr);
  if (FAILED(hr))
    return false;

  if (!params_def.Get()) {
    // You hit this special case if the WMI class is not a CIM class. MSDN
    // sometimes tells you this. Welcome to WMI hell.
    return false;
  }

  hr = params_def->SpawnInstance(0, class_instance->GetAddressOf());
  return SUCCEEDED(hr);
}

bool SetWmiClassMethodParameter(IWbemClassObject* class_method,
                                const StringPiece16& parameter_name,
                                VARIANT* parameter) {
  HRESULT hr = class_method->Put(parameter_name.data(), 0, parameter, 0);
  return SUCCEEDED(hr);
}

// The code in Launch() basically calls the Create Method of the Win32_Process
// CIM class is documented here:
// http://msdn2.microsoft.com/en-us/library/aa389388(VS.85).aspx
// NOTE: The documentation for the Create method suggests that the ProcessId
// parameter and return value are of type uint32_t, but when we call the method
// the values in the returned out_params, are VT_I4, which is int32_t.
bool WmiLaunchProcess(const string16& command_line, int* process_id) {
  ComPtr<IWbemServices> wmi_local;
  if (!CreateLocalWmiConnection(true, &wmi_local))
    return false;

  static constexpr wchar_t class_name[] = L"Win32_Process";
  static constexpr wchar_t method_name[] = L"Create";
  ComPtr<IWbemClassObject> process_create;
  if (!CreateWmiClassMethodObject(wmi_local.Get(), class_name, method_name,
                                  &process_create)) {
    return false;
  }

  ScopedVariant b_command_line(command_line.c_str());

  if (!SetWmiClassMethodParameter(process_create.Get(), L"CommandLine",
                                  b_command_line.AsInput())) {
    return false;
  }

  ComPtr<IWbemClassObject> out_params;
  HRESULT hr = wmi_local->ExecMethod(
      ScopedBstr(class_name), ScopedBstr(method_name), 0, nullptr,
      process_create.Get(), out_params.GetAddressOf(), nullptr);
  if (FAILED(hr))
    return false;

  // We're only expecting int32_t or uint32_t values, so no need for
  // ScopedVariant.
  VARIANT ret_value = {{{VT_EMPTY}}};
  hr = out_params->Get(L"ReturnValue", 0, &ret_value, nullptr, 0);
  if (FAILED(hr) || V_I4(&ret_value) != 0)
    return false;

  VARIANT pid = {{{VT_EMPTY}}};
  hr = out_params->Get(L"ProcessId", 0, &pid, nullptr, 0);
  if (FAILED(hr) || V_I4(&pid) == 0)
    return false;

  if (process_id)
    *process_id = V_I4(&pid);

  return true;
}

WmiComputerSystemInfo WmiComputerSystemInfo::Get() {
  ComPtr<IWbemServices> services;
  if (!CreateLocalWmiConnection(true, &services))
    return WmiComputerSystemInfo();

  ScopedBstr query_language(L"WQL");
  ScopedBstr query(L"SELECT * FROM Win32_ComputerSystem");
  ComPtr<IEnumWbemClassObject> enumerator;
  HRESULT hr =
      services->ExecQuery(query_language, query,
                          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                          nullptr, enumerator.GetAddressOf());
  if (FAILED(hr) || !enumerator.Get())
    return WmiComputerSystemInfo();

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator->Next(WBEM_INFINITE, 1, class_object.GetAddressOf(),
                        &items_returned);
  if (!items_returned)
    return WmiComputerSystemInfo();

  ScopedVariant manufacturer;
  class_object->Get(L"Manufacturer", 0, manufacturer.Receive(), 0, 0);
  ScopedVariant model;
  class_object->Get(L"Model", 0, model.Receive(), 0, 0);

  WmiComputerSystemInfo info;
  if (manufacturer.type() == VT_BSTR)
    info.manufacturer_ = V_BSTR(manufacturer.ptr());

  if (model.type() == VT_BSTR)
    info.model_ = V_BSTR(model.ptr());

  return info;
}

}  // namespace win
}  // namespace base
