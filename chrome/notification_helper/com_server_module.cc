// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT isn't being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/notification_helper/com_server_module.h"

#include <wrl/module.h>

#include "base/metrics/histogram_macros.h"
#include "chrome/install_static/install_util.h"
#include "chrome/notification_helper/notification_activator.h"
#include "chrome/notification_helper/trace_util.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ComServerModuleStatus {
  SUCCESS = 0,
  FACTORY_CREATION_FAILED = 1,
  ICLASSFACTORY_OBJECT_CREATION_FAILED = 2,
  REGISTRATION_FAILED = 3,
  UNREGISTRATION_FAILED = 4,
  COUNT  // Must be the final value.
};

void LogComServerModuleHistogram(ComServerModuleStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.NotificationHelper.ComServerModuleStatus", status,
      ComServerModuleStatus::COUNT);
}

}  // namespace

namespace notification_helper {

// The reset policy of the event MUST BE set to MANUAL to avoid signaling the
// event in IsSignaled() itself, which is called by IsEventSignaled().
ComServerModule::ComServerModule()
    : object_zero_count_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

ComServerModule::~ComServerModule() = default;

HRESULT ComServerModule::Run() {
  HRESULT hr = RegisterClassObjects();
  if (SUCCEEDED(hr)) {
    WaitForZeroObjectCount();
    hr = UnregisterClassObjects();
  }
  if (SUCCEEDED(hr))
    LogComServerModuleHistogram(ComServerModuleStatus::SUCCESS);

  return hr;
}

HRESULT ComServerModule::RegisterClassObjects() {
  // Create an out-of-proc COM module with caching disabled. The supplied
  // method is invoked when the last instance object of the module is released.
  auto& module =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProcDisableCaching>::Create(
          this, &ComServerModule::SignalObjectCountZero);

  // Usually COM module classes statically define their CLSID at compile time
  // through the use of various macros, and WRL::Module internals takes care of
  // creating the class objects and registering them. However, we need to
  // register the same object with different CLSIDs depending on a runtime
  // setting, so we handle that logic here.

  Microsoft::WRL::ComPtr<IUnknown> factory;
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProcDisableCaching;

  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<NotificationActivator>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LogComServerModuleHistogram(ComServerModuleStatus::FACTORY_CREATION_FAILED);
    Trace(L"%hs(Factory creation failed; hr: 0x%08X)\n", __func__, hr);
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory;
  hr = factory.As(&class_factory);
  if (FAILED(hr)) {
    LogComServerModuleHistogram(
        ComServerModuleStatus::ICLASSFACTORY_OBJECT_CREATION_FAILED);
    Trace(L"%hs(IClassFactory object creation failed; hr: 0x%08X)\n", __func__,
          hr);
    return hr;
  }

  // All pointers in this array are unowned. Do not release them.
  IClassFactory* class_factories[] = {class_factory.Get()};
  static_assert(arraysize(cookies_) == arraysize(class_factories),
                "Arrays cookies_ and class_factories must be the same size.");

  IID class_ids[] = {install_static::GetToastActivatorClsid()};
  static_assert(arraysize(cookies_) == arraysize(class_ids),
                "Arrays cookies_ and class_ids must be the same size.");

  hr = module.RegisterCOMObject(nullptr, class_ids, class_factories, cookies_,
                                arraysize(cookies_));
  if (FAILED(hr)) {
    LogComServerModuleHistogram(ComServerModuleStatus::REGISTRATION_FAILED);
    Trace(L"%hs(NotificationActivator registration failed; hr: 0x%08X)\n",
          __func__, hr);
  }

  return hr;
}

HRESULT ComServerModule::UnregisterClassObjects() {
  auto& module = Microsoft::WRL::Module<
      Microsoft::WRL::OutOfProcDisableCaching>::GetModule();
  HRESULT hr =
      module.UnregisterCOMObject(nullptr, cookies_, arraysize(cookies_));
  if (FAILED(hr)) {
    LogComServerModuleHistogram(ComServerModuleStatus::UNREGISTRATION_FAILED);
    Trace(L"%hs(NotificationActivator unregistration failed; hr: 0x%08X)\n",
          __func__, hr);
  }
  return hr;
}

bool ComServerModule::IsEventSignaled() {
  return object_zero_count_.IsSignaled();
}

void ComServerModule::WaitForZeroObjectCount() {
  object_zero_count_.Wait();
}

void ComServerModule::SignalObjectCountZero() {
  object_zero_count_.Signal();
}

}  // namespace notification_helper
