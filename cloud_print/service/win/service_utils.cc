// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/service_utils.h"

#include <windows.h>
#include <security.h>  // NOLINT

string16 GetCurrentUserName() {
  ULONG size = 0;
  string16 result;
  ::GetUserNameEx(::NameSamCompatible, NULL, &size);
  result.resize(size);
  if (result.empty())
    return result;
  if (!::GetUserNameEx(::NameSamCompatible, &result[0], &size))
    result.clear();
  result.resize(size);
  return result;
}


