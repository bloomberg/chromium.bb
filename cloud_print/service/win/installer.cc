// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/installer.h"

#include <winerror.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/shortcut.h"
#include "cloud_print/common/win/cloud_print_utils.h"
#include "cloud_print/common/win/install_utils.h"
#include "cloud_print/resources.h"
#include "cloud_print/service/service_constants.h"
#include "cloud_print/service/service_switches.h"
#include "cloud_print/service/win/service_controller.h"

namespace {

const wchar_t kConfigBinaryName[] = L"cloud_print_service_config.exe";

base::FilePath GetShortcutPath(int dir_key, bool with_subdir) {
  base::FilePath path;
  if (!PathService::Get(dir_key, &path))
    return base::FilePath();
  path = path.Append(cloud_print::LoadLocalString(IDS_FULL_PRODUCT_NAME));
  if (with_subdir)
    path = path.Append(cloud_print::LoadLocalString(IDS_FULL_PRODUCT_NAME));
  return path.InsertBeforeExtension(L".lnk");
}

void CreateShortcut(int dir_key, bool with_subdir,
                    base::win::ShortcutOperation operation) {
  base::FilePath path = GetShortcutPath(dir_key, with_subdir);
  if (path.empty())
    return;
  base::CreateDirectory(path.DirName());
  base::win::ShortcutProperties properties;

  base::FilePath exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return;
  exe_path = exe_path.DirName().Append(base::FilePath(kConfigBinaryName));
  properties.set_target(exe_path);
  properties.set_working_dir(exe_path.DirName());
  CreateOrUpdateShortcutLink(path, properties, operation);
}

void CreateShortcuts(bool create_always) {
  base::win::ScopedCOMInitializer co_init;
  base::win::ShortcutOperation operation =
      create_always ? base::win::SHORTCUT_CREATE_ALWAYS :
                      base::win::SHORTCUT_REPLACE_EXISTING;
  CreateShortcut(base::DIR_COMMON_START_MENU, true, operation);
  CreateShortcut(base::DIR_COMMON_DESKTOP, false, operation);
}

void DeleteShortcut(int dir_key, bool with_subdir) {
  base::FilePath path = GetShortcutPath(dir_key, with_subdir);
  if (path.empty())
    return;
  if (with_subdir)
    base::DeleteFile(path.DirName(), true);
  else
    base::DeleteFile(path, false);
}

void DeleteShortcuts() {
  DeleteShortcut(base::DIR_COMMON_START_MENU, true);
  DeleteShortcut(base::DIR_COMMON_DESKTOP, false);
}

}  // namespace

HRESULT ProcessInstallerSwitches() {
  const CommandLine& command_line(*CommandLine::ForCurrentProcess());

  if (command_line.HasSwitch(kInstallSwitch)) {
    base::FilePath old_location =
        cloud_print::GetInstallLocation(kGoogleUpdateId);

    cloud_print::CreateUninstallKey(
        kGoogleUpdateId, cloud_print::LoadLocalString(IDS_FULL_PRODUCT_NAME),
        kUninstallSwitch);

    ServiceController controller;
    HRESULT hr = controller.UpdateBinaryPath();
    if (FAILED(hr))
      return hr;

    if (!old_location.empty() &&
        cloud_print::IsProgramsFilesParent(old_location) &&
        old_location != cloud_print::GetInstallLocation(kGoogleUpdateId)) {
      base::DeleteFile(old_location, true);
    }

    cloud_print::SetGoogleUpdateKeys(
        kGoogleUpdateId, cloud_print::LoadLocalString(IDS_FULL_PRODUCT_NAME));

    CreateShortcuts(old_location.empty());

    return S_OK;
  } else if (command_line.HasSwitch(kUninstallSwitch)) {
    ServiceController controller;
    HRESULT hr = controller.UninstallService();
    if (FAILED(hr))
      return hr;

    DeleteShortcuts();

    cloud_print::DeleteGoogleUpdateKeys(kGoogleUpdateId);
    cloud_print::DeleteUninstallKey(kGoogleUpdateId);
    cloud_print::DeleteProgramDir(kDeleteSwitch);
    return S_OK;
  } else if (command_line.HasSwitch(kDeleteSwitch)) {
    base::FilePath delete_path = command_line.GetSwitchValuePath(kDeleteSwitch);
    if (!delete_path.empty() &&
        cloud_print::IsProgramsFilesParent(delete_path)) {
      base::DeleteFile(delete_path, true);
    }
    return S_OK;
  }

  return S_FALSE;
}

class CloudPrintServiceSetupModule
    : public ATL::CAtlExeModuleT<CloudPrintServiceSetupModule> {
};

CloudPrintServiceSetupModule _AtlModule;

int WINAPI WinMain(__in  HINSTANCE hInstance,
                   __in  HINSTANCE hPrevInstance,
                   __in  LPSTR lpCmdLine,
                   __in  int nCmdShow) {
  base::AtExitManager at_exit;
  CommandLine::Init(0, NULL);
  return ProcessInstallerSwitches();
}
