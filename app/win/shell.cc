// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/win/shell.h"

#include <shellapi.h>
#include <shlobj.h>

#include "base/file_path.h"
#include "base/native_library.h"
#include "base/string_util.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace app {
namespace win {

namespace {

const wchar_t kShell32[] = L"shell32.dll";
const char kSHGetPropertyStoreForWindow[] = "SHGetPropertyStoreForWindow";

// Define the type of SHGetPropertyStoreForWindow is SHGPSFW.
typedef DECLSPEC_IMPORT HRESULT (STDAPICALLTYPE *SHGPSFW)(HWND hwnd,
                                                          REFIID riid,
                                                          void** ppv);

}  // namespace

// Open an item via a shell execute command. Error code checking and casting
// explanation: http://msdn2.microsoft.com/en-us/library/ms647732.aspx
bool OpenItemViaShell(const FilePath& full_path) {
  HINSTANCE h = ::ShellExecuteW(
      NULL, NULL, full_path.value().c_str(), NULL,
      full_path.DirName().value().c_str(), SW_SHOWNORMAL);

  LONG_PTR error = reinterpret_cast<LONG_PTR>(h);
  if (error > 32)
    return true;

  if ((error == SE_ERR_NOASSOC))
    return OpenItemWithExternalApp(full_path.value());

  return false;
}

bool OpenItemViaShellNoZoneCheck(const FilePath& full_path) {
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_NOZONECHECKS | SEE_MASK_FLAG_DDEWAIT;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = NULL;
  sei.lpFile = full_path.value().c_str();
  if (::ShellExecuteExW(&sei))
    return true;
  LONG_PTR error = reinterpret_cast<LONG_PTR>(sei.hInstApp);
  if ((error == SE_ERR_NOASSOC))
    return OpenItemWithExternalApp(full_path.value());
  return false;
}

// Show the Windows "Open With" dialog box to ask the user to pick an app to
// open the file with.
bool OpenItemWithExternalApp(const string16& full_path) {
  SHELLEXECUTEINFO sei = { sizeof(sei) };
  sei.fMask = SEE_MASK_FLAG_DDEWAIT;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = L"openas";
  sei.lpFile = full_path.c_str();
  return (TRUE == ::ShellExecuteExW(&sei));
}

void SetAppIdForWindow(const string16& app_id, HWND hwnd) {
  // This functionality is only available on Win7+.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  // Load Shell32.dll into memory.
  // TODO(brg): Remove this mechanism when the Win7 SDK is available in trunk.
  std::wstring shell32_filename(kShell32);
  FilePath shell32_filepath(shell32_filename);
  base::NativeLibrary shell32_library = base::LoadNativeLibrary(
      shell32_filepath);

  if (!shell32_library)
    return;

  // Get the function pointer for SHGetPropertyStoreForWindow.
  void* function = base::GetFunctionPointerFromNativeLibrary(
      shell32_library,
      kSHGetPropertyStoreForWindow);

  if (!function) {
    base::UnloadNativeLibrary(shell32_library);
    return;
  }

  // Set the application's name.
  base::win::ScopedComPtr<IPropertyStore> pps;
  SHGPSFW SHGetPropertyStoreForWindow = static_cast<SHGPSFW>(function);
  HRESULT result = SHGetPropertyStoreForWindow(
      hwnd, __uuidof(*pps), reinterpret_cast<void**>(pps.Receive()));
  if (S_OK == result)
    base::win::SetAppIdForPropertyStore(pps, app_id.c_str());

  // Cleanup.
  base::UnloadNativeLibrary(shell32_library);
}

}  // namespace win
}  // namespace app
