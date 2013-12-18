// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/lsb_release_log_source.h"

#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"

namespace system_logs {

void LsbReleaseLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(!callback.is_null());
  scoped_ptr<SystemLogsResponse> response(new SystemLogsResponse);
  const base::SysInfo::LsbReleaseMap& lsb_map =
      base::SysInfo::GetLsbReleaseMap();
  for (base::SysInfo::LsbReleaseMap::const_iterator iter = lsb_map.begin();
       iter != lsb_map.end(); ++iter) {
    (*response)[iter->first] = iter->second;
  }
  callback.Run(response.get());
}

}  // namespace system_logs
