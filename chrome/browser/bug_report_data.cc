// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bug_report_data.h"

#include "chrome/browser/bug_report_util.h"
#include "content/browser/browser_thread.h"

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
                               ScreenshotDataPtr image
#if defined(OS_CHROMEOS)
                               , const std::string& user_email
                               , const bool send_sys_info
                               , const bool sent_report
#endif
                               ) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void BugReportData::SendReport() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_CHROMEOS)
  if (sent_report_)
    return;  // We already received the syslogs and sent the report.

  // Set send_report_ to ensure that we only send one report.
  sent_report_ = true;
#endif

  gfx::Rect& screen_size = BugReportUtil::GetScreenshotSize();
  BugReportUtil::SendReport(profile_
                            , problem_type_
                            , page_url_
                            , description_
                            , image_
                            , screen_size.width()
                            , screen_size.height()
#if defined(OS_CHROMEOS)
                            , user_email_
                            , zip_content_ ? zip_content_->c_str() : NULL
                            , zip_content_ ? zip_content_->length() : 0
                            , send_sys_info_ ? sys_info_ : NULL
#endif
                            );

#if defined(OS_CHROMEOS)
  if (sys_info_) {
    delete sys_info_;
    sys_info_ = NULL;
  }
  if (zip_content_) {
    delete zip_content_;
    zip_content_ = NULL;
  }
#endif

  // Delete this object once the report has been sent.
  delete this;
}

#if defined(OS_CHROMEOS)
// SyslogsComplete may be called before UpdateData, in which case, we do not
// want to delete the logs that were gathered, and we do not want to send the
// report either. Instead simply populate |sys_info_| and |zip_content_|.
void BugReportData::SyslogsComplete(chromeos::system::LogDictionaryType* logs,
                                    std::string* zip_content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
