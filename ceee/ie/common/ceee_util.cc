// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions for CEEE.

#include "ceee/ie/common/ceee_util.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/win/registry.h"

#include "toolband.h"  // NOLINT

namespace ceee_util {

bool IsIeCeeeRegistered() {
  wchar_t clsid[40] = { 0 };
  int num_chars = ::StringFromGUID2(CLSID_ToolBand, clsid, arraysize(clsid));
  CHECK(num_chars > 0);

  std::wstring path(L"CLSID\\");
  path += clsid;

  base::win::RegKey key;
  return key.Open(HKEY_CLASSES_ROOT, path.c_str(), KEY_QUERY_VALUE);
}

}  // namespace ceee_util
