// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_internal.h"

#include <windows.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "base/file_path.h"
#include "base/win/metro.h"
#include "base/path_service.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/common/content_switches.h"

namespace chrome {

bool GetDefaultUserDataDirectory(FilePath* result) {
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  *result = result->Append(dist->GetInstallSubDir());
  if (base::win::GetMetroModule())
    *result = result->Append(kMetroChromeUserDataSubDir);
  *result = result->Append(chrome::kUserDataDirname);
  return true;
}

bool GetChromeFrameUserDataDirectory(FilePath* result) {
  if (!PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME);
  *result = result->Append(dist->GetInstallSubDir());
  *result = result->Append(chrome::kUserDataDirname);
  return true;
}

void GetUserCacheDirectory(const FilePath& profile_dir, FilePath* result) {
  // This function does more complicated things on Mac/Linux.
  *result = profile_dir;
}

bool GetUserDocumentsDirectory(FilePath* result) {
  wchar_t path_buf[MAX_PATH];
  if (FAILED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL,
                             SHGFP_TYPE_CURRENT, path_buf)))
    return false;
  *result = FilePath(path_buf);
  return true;
}

// Return a default path for downloads that is safe.
// We just use 'Downloads' under DIR_USER_DOCUMENTS. Localizing
// 'downloads' is not a good idea because Chrome's UI language
// can be changed.
bool GetUserDownloadsDirectorySafe(FilePath* result) {
  if (!GetUserDocumentsDirectory(result))
    return false;

  *result = result->Append(L"Downloads");
  return true;
}

// On Vista and higher, use the downloads known folder. Since it can be
// relocated to point to a "dangerous" folder, callers should validate that the
// returned path is not dangerous before using it.
bool GetUserDownloadsDirectory(FilePath* result) {
  typedef HRESULT (WINAPI *GetKnownFolderPath)(
      REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
  GetKnownFolderPath f = reinterpret_cast<GetKnownFolderPath>(
      GetProcAddress(GetModuleHandle(L"shell32.dll"), "SHGetKnownFolderPath"));
  base::win::ScopedCoMem<wchar_t> path_buf;
  if (f && SUCCEEDED(f(FOLDERID_Downloads, 0, NULL, &path_buf))) {
    *result = FilePath(std::wstring(path_buf));
    return true;
  }
  return GetUserDownloadsDirectorySafe(result);
}

bool GetUserDesktop(FilePath* result) {
  // We need to go compute the value. It would be nice to support paths
  // with names longer than MAX_PATH, but the system functions don't seem
  // to be designed for it either, with the exception of GetTempPath
  // (but other things will surely break if the temp path is too long,
  // so we don't bother handling it.
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  if (FAILED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
                             SHGFP_TYPE_CURRENT, system_buffer)))
    return false;
  *result = FilePath(system_buffer);
  return true;
}

bool ProcessNeedsProfileDir(const std::string& process_type) {
  // On windows we don't want subprocesses other than the browser process and
  // service processes to be able to use the profile directory because if it
  // lies on a network share the sandbox will prevent us from accessing it.
  // TODO(pastarmovj): For now gpu and plugin broker processes are whitelisted
  // too because they do use the profile dir in some way but this must be
  // investigated and fixed if possible.
  return process_type.empty() ||
         process_type == switches::kServiceProcess ||
         process_type == switches::kGpuProcess ||
         process_type == switches::kNaClBrokerProcess ||
         process_type == switches::kNaClLoaderProcess ||
         process_type == switches::kPpapiBrokerProcess;
}

}  // namespace chrome
