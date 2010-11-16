// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_data.h"

#include "chrome/browser/ui/browser.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/notifications/system_notification.h"
#endif



#if defined(OS_CHROMEOS)
// Called from the same thread as HandleGetDialogDefaults, i.e. the UI thread.
void BugReportData::SyslogsComplete(chromeos::LogDictionaryType* logs,
                                    std::string* zip_content) {
  if (sent_report_) {
    // We already sent the report, just delete the data.
    if (logs)
      delete logs;
    if (zip_content)
      delete zip_content;
  } else {
    zip_content_ = zip_content;
    sys_info_ = logs;  // Will get deleted when SendReport() is called.
    if (send_sys_info_) {
      // We already prepared the report, send it now.
      this->SendReport();
    }
  }
}
#endif
