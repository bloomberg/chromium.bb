// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/mdm_utils.h"

#include <windows.h>

#include "base/stl_util.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

constexpr wchar_t kRegMdmUrl[] = L"mdm";
constexpr wchar_t kRegMdmSupportsMultiUser[] = L"mdm_mu";

bool MdmEnrollmentEnabled() {
  wchar_t mdm_url[256];
  ULONG length = base::size(mdm_url);
  HRESULT hr = GetGlobalFlag(kRegMdmUrl, mdm_url, &length);

  return hr == S_OK ? mdm_url[0] != '\0' : false;
}

}  // namespace credential_provider
