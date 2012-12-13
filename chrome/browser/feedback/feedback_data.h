// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_

#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/webui/screenshot_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/syslogs_provider.h"
#endif

class Profile;

class FeedbackData {
 public:
  FeedbackData();
  ~FeedbackData();

  void SendReport();

  void UpdateData(Profile* profile,
                  const std::string& target_tab_url,
                  const std::string& category_tag,
                  const std::string& page_url,
                  const std::string& description,
                  const std::string& user_email,
                  ScreenshotDataPtr image
#if defined(OS_CHROMEOS)
                  , const bool send_sys_info
                  , const bool sent_report
                  , const std::string& timestamp
                  , const std::string& attached_filename
                  , const std::string& attached_filedata
#endif
                  );

#if defined(OS_CHROMEOS)
  void SyslogsComplete(chromeos::system::LogDictionaryType* logs,
                       std::string* zip_content);
#endif

  const std::string& target_tab_url() const { return target_tab_url_; }

  const std::string& category_tag() const { return category_tag_; }
  const std::string& page_url() const { return page_url_; }
  const std::string& description() const { return description_; }
  const std::string& user_email() const { return user_email_; }
  ScreenshotDataPtr image() const { return image_; }
#if defined(OS_CHROMEOS)
  chromeos::system::LogDictionaryType* sys_info() const { return sys_info_; }
  bool send_sys_info() const { return send_sys_info_; }
  bool sent_report() const { return sent_report_; }
  std::string* zip_content() const { return zip_content_; }
#endif


 private:
  Profile* profile_;

  // Target tab url.
  std::string target_tab_url_;

  std::string category_tag_;
  std::string page_url_;
  std::string description_;
  std::string user_email_;
  ScreenshotDataPtr image_;

#if defined(OS_CHROMEOS)
  // Chromeos specific values for SendReport.
  chromeos::system::LogDictionaryType* sys_info_;
  // Content of the compressed system logs.
  std::string* zip_content_;
  std::string timestamp_;
  std::string attached_filename_;
  std::string attached_filedata_;

  // NOTE: Extra boolean sent_report_ is required because callback may
  // occur before or after we call SendReport().
  bool sent_report_;
  // Flag to indicate to SyslogsComplete that it should send the report
  bool send_sys_info_;
#endif
};

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_
