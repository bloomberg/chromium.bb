// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/cloud_print_service.h"

#include "base/win/scoped_handle.h"
#include "cloud_print/service/win/resource.h"

// The traits class for Windows Service.
class ServiceHandleTraits {
 public:
  typedef SC_HANDLE Handle;

  // Closes the handle.
  static bool CloseHandle(Handle handle) {
    return ::CloseServiceHandle(handle) != FALSE;
  }

  static bool IsHandleValid(Handle handle) {
    return handle != NULL;
  }

  static Handle NullHandle() {
    return NULL;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceHandleTraits);
};

typedef base::win::GenericScopedHandle<ServiceHandleTraits> ServiceHandle;

HRESULT HResultFromLastError() {
  HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
  // Something already failed if function called.
  if (SUCCEEDED(hr))
    hr = E_FAIL;
  return hr;
}

class CloudPrintServiceModule
    : public ATL::CAtlServiceModuleT<CloudPrintServiceModule, IDS_SERVICENAME> {
 public:
  typedef ATL::CAtlServiceModuleT<CloudPrintServiceModule,
                                  IDS_SERVICENAME> Base;

  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CLOUDPRINTSERVICE,
                                    "{8013FB7C-2E3E-4992-B8BD-05C0C4AB0627}")

  HRESULT InitializeSecurity() {
    // TODO(gene): Check if we need to call CoInitializeSecurity and provide
    // the appropriate security settings for service.
    return S_OK;
  }

  // Override to set autostart and start service.
  HRESULT RegisterAppId(bool bService = false) {
    HRESULT hr = Base::RegisterAppId(bService);
    if (FAILED(hr))
      return hr;

    ServiceHandle service;
    hr = OpenService(SERVICE_CHANGE_CONFIG, &service);
    if (FAILED(hr))
      return hr;

    if (!::ChangeServiceConfig(service, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, NULL, NULL, NULL, NULL,
        L"NT AUTHORITY\\LocalService", NULL, NULL)) {
      return HResultFromLastError();
    }

    return S_OK;
  }

  // Override to handle service uninstall case manually.
  bool ParseCommandLine(LPCTSTR lpCmdLine, HRESULT* pnRetCode) {
    if (!Base::ParseCommandLine(lpCmdLine, pnRetCode))
      return false;

    const wchar_t tokens[] = L"-/";
    *pnRetCode = S_OK;

    for (const wchar_t* cur = lpCmdLine; cur = FindOneOf(cur, tokens);) {
      if (WordCmpI(cur, L"UninstallService") == 0) {
        if (!Uninstall())
          *pnRetCode = E_FAIL;
        return false;
      } else if (WordCmpI(cur, L"Start") == 0) {
        *pnRetCode = StartService();
        return false;
      } else if (WordCmpI(cur, L"Stop") == 0) {
        *pnRetCode = StopService();
        return false;
      }
    }
    return true;
  }

  HRESULT PreMessageLoop(int nShowCmd) {
    HRESULT hr = Base::PreMessageLoop(nShowCmd);
    if (FAILED(hr))
      return hr;

    hr = StartConnector();
    if (FAILED(hr))
      return hr;

    LogEvent(_T("Service started/resumed"));
    SetServiceStatus(SERVICE_RUNNING);
    return hr;
  }

  HRESULT PostMessageLoop() {
    StopConnector();
    return Base::PostMessageLoop();
  }

 private:
  HRESULT OpenService(DWORD access, ServiceHandle* service) {
    if (!service)
      return E_POINTER;

    ServiceHandle scm(::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!scm.IsValid())
      return HResultFromLastError();

    service->Set(::OpenService(scm, m_szServiceName, access));

    if (!service->IsValid())
      return HResultFromLastError();

    return S_OK;
  }

  HRESULT StartService() {
    ServiceHandle service;
    HRESULT hr = OpenService(SERVICE_START, &service);
    if (FAILED(hr))
      return hr;
    if (!::StartService(service, 0, NULL))
      return HResultFromLastError();
    return S_OK;
  }

  HRESULT StopService() {
    ServiceHandle service;
    HRESULT hr = OpenService(SERVICE_STOP, &service);
    if (FAILED(hr))
      return hr;
    SERVICE_STATUS status = {0};
    if (!::ControlService(service, SERVICE_CONTROL_STOP, &status))
      return HResultFromLastError();
    return S_OK;
  }

  HRESULT StartConnector() {
    return S_OK;
  }

  void StopConnector() {
  }
};

CloudPrintServiceModule _AtlModule;

int WINAPI WinMain(__in  HINSTANCE hInstance,
                   __in  HINSTANCE hPrevInstance,
                   __in  LPSTR lpCmdLine,
                   __in  int nCmdShow) {
  return _AtlModule.WinMain(nCmdShow);
}

