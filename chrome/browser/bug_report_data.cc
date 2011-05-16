// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_data.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/notifications/system_notification.h"
#endif

BugReportData::BugReportData()
    : profile_(NULL),
      problem_type_(0)
#if defined(OS_CHROMEOS)
    , sys_info_(NULL)
    , zip_content_(NULL)
    , sent_report_(false)
    , send_sys_info_(false)
#endif
{
}

BugReportData::~BugReportData() {}

void BugReportData::UpdateData(Profile* profile,
                               const std::string& target_tab_url,
                               const int problem_type,
                               const std::string& page_url,
                               const std::string& description,
                               const std::vector<unsigned char>& image
#if defined(OS_CHROMEOS)
                               , const std::string& user_email
                               , const bool send_sys_info
                               , const bool sent_report
#endif
                               ) {
  profile_ = profile;
  target_tab_url_ = target_tab_url;
  problem_type_ = problem_type;
  page_url_ = page_url;
  description_ = description;
  image_ = image;
#if defined(OS_CHROMEOS)
  user_email_ = user_email;
  send_sys_info_ = send_sys_info;
  sent_report_ = sent_report;
#endif
}


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
