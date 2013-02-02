// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/touch_log_source.h"

#include "ash/shell.h"
#include "ash/touch/touch_observer_hud.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

TouchLogSource::TouchLogSource() {
}

TouchLogSource::~TouchLogSource() {
}

void TouchLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse response;
  if (ash::Shell::GetInstance()->touch_observer_hud()) {
    response[kHUDLogDataKey] =
        ash::Shell::GetInstance()->touch_observer_hud()->GetLogAsString();
  }
  callback.Run(&response);
}

}  // namespace chromeos
