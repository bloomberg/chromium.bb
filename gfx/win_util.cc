// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/win_util.h"

#include <windows.h>

#include "base/win_util.h"

namespace {

bool DynamicLibraryPresent(const wchar_t* lib_name) {
  HINSTANCE lib = LoadLibrary(lib_name);
  if (lib) {
    FreeLibrary(lib);
    return true;
  }
  return false;
}

}  // namespace

namespace gfx {

// Returns true if Direct2d is available, false otherwise.
bool Direct2dIsAvailable() {
  static bool checked = false;
  static bool available = false;

  if (!checked) {
    win_util::WinVersion version = win_util::GetWinVersion();
    if (version < win_util::WINVERSION_VISTA)
      available = false;
    else if (version >= win_util::WINVERSION_WIN7)
      available = true;
    else
      available = DynamicLibraryPresent(L"d2d1.dll");
    checked = true;
  }
  return available;
}

// Returns true if DirectWrite is available, false otherwise.
bool DirectWriteIsAvailable() {
  static bool checked = false;
  static bool available = false;

  if (!checked) {
    win_util::WinVersion version = win_util::GetWinVersion();
    if (version < win_util::WINVERSION_VISTA)
      available = false;
    else if (version >= win_util::WINVERSION_WIN7)
      available = true;
    else
      available = DynamicLibraryPresent(L"dwrite.dll");
    checked = true;
  }
  return available;
}

}  // namespace gfx

