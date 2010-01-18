// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/win_util.h"

namespace win_util {

bool ConvertToLongPath(const std::wstring& short_path,
                       std::wstring* long_path) {
  wchar_t long_path_buf[MAX_PATH];
  DWORD return_value = GetLongPathName(short_path.c_str(), long_path_buf,
                                       MAX_PATH);
  if (return_value != 0 && return_value < MAX_PATH) {
    *long_path = long_path_buf;
    return true;
  }

  return false;
}

}  // namespace win_util
