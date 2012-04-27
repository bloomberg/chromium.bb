// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/cloud_print_service.h"

#include "cloud_print/service/win/resource.h"

class CloudPrintServiceModule
  : public ATL::CAtlServiceModuleT<CloudPrintServiceModule, IDS_SERVICENAME> {
 public:
  DECLARE_REGISTRY_APPID_RESOURCEID(IDR_CLOUDPRINTSERVICE,
                                    "{8013FB7C-2E3E-4992-B8BD-05C0C4AB0627}")
  HRESULT InitializeSecurity() throw() {
    // TODO(gene): Check if we need to call CoInitializeSecurity and provide
    // the appropriate security settings for service.
    return S_OK;
  }
};

CloudPrintServiceModule _AtlModule;

int WINAPI WinMain(__in  HINSTANCE hInstance,
                   __in  HINSTANCE hPrevInstance,
                   __in  LPSTR lpCmdLine,
                   __in  int nCmdShow) {
  // Handle service unstall case manually.
  // Service install is handled through ATL, command line flag "/Service"
  // http://msdn.microsoft.com/en-US/library/z8868y94(v=vs.80).aspx
  if (StrStrA(lpCmdLine, "/UninstallService") != NULL) {
    _AtlModule.Uninstall();
    return 0;
  }

  // TODO(gene): When running this program with "/Service" flag it fails
  // silently if command prompt is not elevated.
  // Consider adding manifest to require elevated prompt, or at least
  // print warning in this case.
  return _AtlModule.WinMain(nCmdShow);
}

