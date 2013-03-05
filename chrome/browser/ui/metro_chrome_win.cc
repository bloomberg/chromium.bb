// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/metro_chrome_win.h"

#include <windows.h>
#include <shobjidl.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"

namespace chrome {

bool ActivateMetroChrome() {
  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Failed to get chrome exe path";
    return false;
  }

  string16 app_id = ShellUtil::GetBrowserModelId(
      BrowserDistribution::GetDistribution(),
      InstallUtil::IsPerUserInstall(chrome_exe.value().c_str()));
  if (app_id.empty()) {
    NOTREACHED() << "Failed to get chrome app user model id.";
    return false;
  }

  base::win::ScopedComPtr<IApplicationActivationManager> activation_manager;
  HRESULT hr = activation_manager.CreateInstance(
      CLSID_ApplicationActivationManager);
  if (!activation_manager) {
    NOTREACHED() << "Failed to cocreate activation manager. Error: " << hr;
    return false;
  }

  unsigned long pid = 0;
  hr = activation_manager->ActivateApplication(app_id.c_str(),
                                               L"open",
                                               AO_NONE,
                                               &pid);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to activate metro chrome. Error: " << hr;
    return false;
  }

  return true;
}

}  // namespace chrome
