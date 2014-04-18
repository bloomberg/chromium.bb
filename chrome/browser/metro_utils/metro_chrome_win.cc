// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metro_utils/metro_chrome_win.h"

#include <windows.h>
#include <shobjidl.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/metro.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"

namespace chrome {

bool ActivateMetroChrome() {
  // TODO(cpu): For Win7 we need to activate differently.
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return true;

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Failed to get chrome exe path";
    return false;
  }

  base::string16 app_id = ShellUtil::GetBrowserModelId(
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
    NOTREACHED() << "Failed to cocreate activation manager. Error: "
                 << std::showbase << std::hex << hr;
    return false;
  }

  unsigned long pid = 0;
  hr = activation_manager->ActivateApplication(app_id.c_str(),
                                               L"open",
                                               AO_NONE,
                                               &pid);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to activate metro chrome. Error: "
                 << std::showbase << std::hex << hr;
    return false;
  }

  return true;
}

Win8Environment GetWin8Environment(HostDesktopType desktop) {
#if defined(USE_AURA) && defined(USE_ASH)
  if (desktop == chrome::HOST_DESKTOP_TYPE_ASH)
    return WIN_8_ENVIRONMENT_METRO_AURA;
  else
    return WIN_8_ENVIRONMENT_DESKTOP_AURA;
#else
  if (base::win::IsProcessImmersive(::GetCurrentProcess()))
    return WIN_8_ENVIRONMENT_METRO;
  else
    return WIN_8_ENVIRONMENT_DESKTOP;
#endif
}


}  // namespace chrome
