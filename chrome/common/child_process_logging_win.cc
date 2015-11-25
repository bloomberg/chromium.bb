// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#include <windows.h>

#include "base/debug/crash_logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/crash_keys.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/client_info.h"

namespace child_process_logging {

void Init() {
  // This would be handled by BreakpadClient::SetCrashClientIdFromGUID(), but
  // because of the aforementioned issue, crash keys aren't ready yet at the
  // time of Breakpad initialization, load the client id backed up in Google
  // Update settings instead.
  scoped_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info)
    crash_keys::SetMetricsClientIdFromGUID(client_info->client_id);
}

}  // namespace child_process_logging
