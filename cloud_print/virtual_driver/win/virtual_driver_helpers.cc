// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

#include <windows.h>
#include <winspool.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/win/windows_version.h"
#include "cloud_print/common/win/cloud_print_utils.h"

namespace cloud_print {

void DisplayWindowsMessage(HWND hwnd,
                           HRESULT hr,
                           const base::string16& caption) {
  ::MessageBox(hwnd, GetErrorMessage(hr).c_str(), caption.c_str(), MB_OK);
}

HRESULT GetPrinterDriverDir(base::FilePath* path) {
  BYTE driver_dir_buffer[MAX_PATH * sizeof(wchar_t)];
  DWORD needed = 0;
  if (!GetPrinterDriverDirectory(NULL, NULL, 1, driver_dir_buffer,
                                 MAX_PATH * sizeof(wchar_t), &needed)) {
    // We could try to allocate a larger buffer if needed > MAX_PATH
    // but that really shouldn't happen.
    return cloud_print::GetLastHResult();
  }
  *path = base::FilePath(reinterpret_cast<wchar_t*>(driver_dir_buffer));

  // The XPS driver is a "Level 3" driver
  *path = path->Append(L"3");
  return S_OK;
}
}
