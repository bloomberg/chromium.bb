// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/touch_log_source.h"

#include "ash/touch/touch_observer_hud.h"
#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"

const char kHUDLogDataKey[] = "hud_log";

namespace chromeos {

TouchLogSource::TouchLogSource() {
}

TouchLogSource::~TouchLogSource() {
}

void TouchLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse response;
  scoped_ptr<DictionaryValue> dictionary =
      ash::internal::TouchObserverHUD::GetAllAsDictionary();
  if (!dictionary->empty()) {
    std::string touch_log;
    JSONStringValueSerializer json(&touch_log);
    json.set_pretty_print(true);
    if (json.Serialize(*dictionary) && !touch_log.empty())
      response[kHUDLogDataKey] = touch_log;
  }
  callback.Run(&response);
}

}  // namespace chromeos
