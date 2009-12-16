// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "googleurl/src/gurl.h"

namespace child_process_logging {

// We use a static string to hold the most recent active url. If we crash, the
// crash handler code will send the contents of this string to the browser.
std::string active_url;

void SetActiveURL(const GURL& url) {
  active_url = url.possibly_invalid_spec();
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  if (str.empty())
    return;

  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

void SetActiveExtensions(const std::set<std::string>& extension_ids) {
  // TODO(port)
}
}  // namespace child_process_logging
