// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/google_update_settings.h"

namespace child_process_logging {

using base::debug::SetCrashKeyValueFuncT;
using base::debug::ClearCrashKeyValueFuncT;
using base::debug::SetCrashKeyValue;
using base::debug::ClearCrashKey;

const char* kGuidParamName = "guid";

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;
static char g_client_id[kClientIdSize];

void SetClientIdImpl(const std::string& client_id,
                     SetCrashKeyValueFuncT set_key_func) {
  set_key_func(kGuidParamName, client_id);
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
    SetClientIdImpl(str, SetCrashKeyValue);

  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

}  // namespace child_process_logging
