// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ICeeeBroker implementation

#include "ceee/ie/broker/broker.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/common/ceee_module_util.h"

HRESULT CeeeBroker::FinalConstruct() {
  // So that we get a pointer to the ExecutorsManager and let tests override it.
  executors_manager_ = Singleton<ExecutorsManager,
                                 ExecutorsManager::SingletonTraits>::get();
  api_dispatcher_ = ProductionApiDispatcher::get();
  return S_OK;
}

void CeeeBroker::OnAddConnection(bool first_lock) {
  if (first_lock)
    ceee_module_util::LockModule();
}

void CeeeBroker::OnReleaseConnection(bool last_unlock,
                                     bool last_unlock_releases) {
  if (last_unlock)
    ceee_module_util::UnlockModule();
  IExternalConnectionImpl<CeeeBroker>::OnReleaseConnection(
      last_unlock, last_unlock_releases);
}

STDMETHODIMP CeeeBroker::Execute(BSTR function, BSTR* response) {
  // This is DEPRECATED and we should use ChromePostman (see FireEvent).
  api_dispatcher_->HandleApiRequest(function, response);
  return S_OK;
}

STDMETHODIMP CeeeBroker::FireEvent(BSTR event_name, BSTR event_args) {
  ChromePostman::GetInstance()->FireEvent(
      WideToUTF8(com::ToString(event_name)).c_str(),
      WideToUTF8(com::ToString(event_args)).c_str());
  return S_OK;
}

STDMETHODIMP CeeeBroker::RegisterWindowExecutor(long thread_id,
                                                IUnknown* executor) {
  // TODO(mad@chromium.org): Add security check here.
  return executors_manager_->RegisterWindowExecutor(thread_id, executor);
}

STDMETHODIMP CeeeBroker::UnregisterExecutor(long thread_id) {
  // TODO(mad@chromium.org): Add security check here.
  return executors_manager_->RemoveExecutor(thread_id);
}

STDMETHODIMP CeeeBroker::RegisterTabExecutor(long thread_id,
                                             IUnknown* executor) {
  // TODO(mad@chromium.org): Implement the proper manual/secure registration.
  return executors_manager_->RegisterTabExecutor(thread_id, executor);
}

STDMETHODIMP CeeeBroker::SetTabIdForHandle(long tab_id,
                                           CeeeWindowHandle handle) {
  // TODO(mad@chromium.org): Add security check here.
  DCHECK(tab_id != kInvalidChromeSessionId &&
      handle != reinterpret_cast<CeeeWindowHandle>(INVALID_HANDLE_VALUE));
  executors_manager_->SetTabIdForHandle(
      static_cast<int>(tab_id), reinterpret_cast<HWND>(handle));
  return S_OK;
}

STDMETHODIMP CeeeBroker::SetTabToolBandIdForHandle(long tool_band_id,
                                                   CeeeWindowHandle handle) {
  // TODO(mad@chromium.org): Add security check here.
  DCHECK(tool_band_id != kInvalidChromeSessionId &&
      handle != reinterpret_cast<CeeeWindowHandle>(INVALID_HANDLE_VALUE));
  executors_manager_->SetTabToolBandIdForHandle(
      static_cast<int>(tool_band_id), reinterpret_cast<HWND>(handle));
  return S_OK;
}
