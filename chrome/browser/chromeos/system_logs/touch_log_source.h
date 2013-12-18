// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_TOUCH_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_TOUCH_LOG_SOURCE_H_

#include "chrome/browser/feedback/system_logs/system_logs_fetcher_base.h"

namespace system_logs {

class TouchLogSource : public SystemLogsSource {
 public:
  TouchLogSource();
  virtual ~TouchLogSource();

 private:
  // Overridden from SystemLogsSource:
  virtual void Fetch(const SysLogsSourceCallback& callback) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TouchLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_TOUCH_LOG_SOURCE_H_
