// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_util.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include "app/gfx/native_widget_types.h"
#include "app/win_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "googleurl/src/gurl.h"

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  FilePath dir = full_path.DirName();
  // ParseDisplayName will fail if the directory is "C:", it must be "C:\\".
  if (dir.value() == L"" || !file_util::EnsureEndsWithSeparator(&dir))
    return;

  typedef HRESULT (WINAPI *SHOpenFolderAndSelectItemsFuncPtr)(
      PCIDLIST_ABSOLUTE pidl_Folder,
      UINT cidl,
      PCUITEMID_CHILD_ARRAY pidls,
      DWORD flags);

  static SHOpenFolderAndSelectItemsFuncPtr open_folder_and_select_itemsPtr =
    NULL;
  static bool initialize_open_folder_proc = true;
  if (initialize_open_folder_proc) {
    initialize_open_folder_proc = false;
    // The SHOpenFolderAndSelectItems API is exposed by shell32 version 6
    // and does not exist in Win2K. We attempt to retrieve this function export
    // from shell32 and if it does not exist, we just invoke ShellExecute to
    // open the folder thus losing the functionality to select the item in
    // the process.
    HMODULE shell32_base = GetModuleHandle(L"shell32.dll");
    if (!shell32_base) {
      NOTREACHED();
      return;
    }
    open_folder_and_select_itemsPtr =
        reinterpret_cast<SHOpenFolderAndSelectItemsFuncPtr>
            (GetProcAddress(shell32_base, "SHOpenFolderAndSelectItems"));
  }
  if (!open_folder_and_select_itemsPtr) {
    ShellExecute(NULL, _T("open"), dir.value().c_str(), NULL, NULL, SW_SHOW);
    return;
  }

  ScopedComPtr<IShellFolder> desktop;
  HRESULT hr = SHGetDesktopFolder(desktop.Receive());
  if (FAILED(hr))
    return;

  win_util::CoMemReleaser<ITEMIDLIST> dir_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
                                 const_cast<wchar_t *>(dir.value().c_str()),
                                 NULL, &dir_item, NULL);
  if (FAILED(hr))
    return;

  win_util::CoMemReleaser<ITEMIDLIST> file_item;
  hr = desktop->ParseDisplayName(NULL, NULL,
      const_cast<wchar_t *>(full_path.value().c_str()),
      NULL, &file_item, NULL);
  if (FAILED(hr))
    return;

  const ITEMIDLIST* highlight[] = {
    {file_item},
  };
  (*open_folder_and_select_itemsPtr)(dir_item, arraysize(highlight),
                                     highlight, NULL);
}

void OpenItem(const FilePath& full_path) {
  win_util::OpenItemViaShell(full_path);
}

void OpenExternal(const GURL& url) {
  // Quote the input scheme to be sure that the command does not have
  // parameters unexpected by the external program. This url should already
  // have been escaped.
  std::string escaped_url = url.spec();
  escaped_url.insert(0, "\"");
  escaped_url += "\"";

  // According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
  // "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
  // ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
  // support URLS of 2083 chars in length, 2K is safe."
  const size_t kMaxUrlLength = 2048;
  if (escaped_url.length() > kMaxUrlLength) {
    NOTREACHED();
    return;
  }

  RegKey key;
  std::wstring registry_path = ASCIIToWide(url.scheme()) +
                               L"\\shell\\open\\command";
  key.Open(HKEY_CLASSES_ROOT, registry_path.c_str());
  if (key.Valid()) {
    DWORD size = 0;
    key.ReadValue(NULL, NULL, &size);
    if (size <= 2) {
      // ShellExecute crashes the process when the command is empty.
      // We check for "2" because it always returns the trailing NULL.
      // TODO(nsylvain): we should also add a dialog to warn on errors. See
      // bug 1136923.
      return;
    }
  }

  if (reinterpret_cast<ULONG_PTR>(ShellExecuteA(NULL, "open",
                                                escaped_url.c_str(), NULL, NULL,
                                                SW_SHOWNORMAL)) <= 32) {
    // We fail to execute the call. We could display a message to the user.
    // TODO(nsylvain): we should also add a dialog to warn on errors. See
    // bug 1136923.
    return;
  }
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return GetAncestor(view, GA_ROOT);
}

string16 GetWindowTitle(gfx::NativeWindow window_handle) {
  std::wstring result;
  int length = ::GetWindowTextLength(window_handle) + 1;
  ::GetWindowText(window_handle, WriteInto(&result, length), length);
  return WideToUTF16(result);
}

bool IsWindowActive(gfx::NativeWindow window) {
  return ::GetForegroundWindow() == window;
}

bool IsVisible(gfx::NativeView view) {
  // MSVC complains if we don't include != 0.
  return ::IsWindowVisible(view) != 0;
}

void SimpleErrorBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message) {
  win_util::MessageBox(parent, message, title, MB_OK | MB_SETFOREGROUND);
}


namespace {

std::wstring CurrentChromeChannel() {
  // See if we can find the Clients key on the HKLM branch.
  HKEY registry_hive = HKEY_LOCAL_MACHINE;
  std::wstring key = google_update::kRegPathClients + std::wstring(L"\\") +
                     google_update::kChromeUpgradeCode;
  RegKey google_update_hklm(registry_hive, key.c_str(), KEY_READ);
  if (!google_update_hklm.Valid()) {
    // HKLM failed us, try the same for the HKCU branch.
    registry_hive = HKEY_CURRENT_USER;
    RegKey google_update_hkcu(registry_hive, key.c_str(), KEY_READ);
    if (!google_update_hkcu.Valid()) {
      // Unknown.
      registry_hive = 0;
    }
  }

  std::wstring update_branch(L"unknown"); // the default.
  if (registry_hive != 0) {
    // Now that we know which hive to use, read the 'ap' key from it.
    std::wstring key = google_update::kRegPathClientState +
                       std::wstring(L"\\") + google_update::kChromeUpgradeCode;
    RegKey client_state(registry_hive, key.c_str(), KEY_READ);
    client_state.ReadValue(google_update::kRegApField, &update_branch);
    // If the parent folder exists (we have a valid install) but the
    // 'ap' key is empty, we necessarily are the stable channel.
    // So we print nothing.
    if (update_branch.empty() && client_state.Valid()) {
      update_branch = L"";
    }
  }

  // Map to something pithy for human consumption. There are no rules as to
  // what the ap string can contain, but generally it will contain a number
  // followed by a dash followed by the branch name (and then some random
  // suffix). We fall back on empty string in case we fail to parse.
  // Only ever return "", "unknown", "dev" or "beta".
  if (update_branch.find(L"-beta") != std::wstring::npos)
    update_branch = L"beta";
  else if (update_branch.find(L"-dev") != std::wstring::npos)
    update_branch = L"dev";
  else if (!update_branch.empty())
    update_branch = L"unknown";

  return update_branch;
}

}  // namespace

string16 GetVersionStringModifier() {
#if defined(GOOGLE_CHROME_BUILD)
  return CurrentChromeChannel();
#else
  return string16();
#endif
}

}  // namespace platform_util
