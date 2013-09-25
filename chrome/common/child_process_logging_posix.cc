// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/installer/util/google_update_settings.h"

namespace child_process_logging {

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;

// We use static strings to hold the most recent active url and the client
// identifier. If we crash, the crash handler code will send the contents of
// these strings to the browser.
char g_client_id[kClientIdSize];

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", std::string());

  if (str.empty())
    return;

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

}  // namespace child_process_logging
