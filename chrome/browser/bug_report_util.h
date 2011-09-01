// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUG_REPORT_UTIL_H_
#define CHROME_BROWSER_BUG_REPORT_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "chrome/browser/userfeedback/proto/common.pb.h"
#include "chrome/browser/userfeedback/proto/extension.pb.h"
#include "chrome/browser/userfeedback/proto/math.pb.h"
#include "ui/gfx/rect.h"

#if defined(OS_MACOSX)
#include "base/sys_info.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/system/syslogs_provider.h"
#endif

class Profile;
class TabContents;

class BugReportUtil {
 public:

#if defined(OS_MACOSX)
  enum BugType {
    PAGE_WONT_LOAD = 0,
    PAGE_LOOKS_ODD,
    PHISHING_PAGE,
    CANT_SIGN_IN,
    CHROME_MISBEHAVES,
    SOMETHING_MISSING,
    BROWSER_CRASH,
    OTHER_PROBLEM
  };
#endif


  // SetOSVersion copies the maj.minor.build + servicePack_string
  // into a string. We currently have:
  //   base::win::GetVersion returns WinVersion, which is just
  //     an enum of 2000, XP, 2003, or VISTA. Not enough detail for
  //     bug reports.
  //   base::SysInfo::OperatingSystemVersion returns an std::string
  //     but doesn't include the build or service pack. That function
  //     is probably the right one to extend, but will require changing
  //     all the call sites or making it a wrapper around another util.
  static void SetOSVersion(std::string *os_version);

  // Send the feedback report after the specified delay
  static void DispatchFeedback(Profile* profile, std::string* feedback_data,
                               int64 delay);


  // Generates bug report data.
  static void SendReport(
      Profile* profile
      , int problem_type
      , const std::string& page_url_text
      , const std::string& description
      , ScreenshotDataPtr png_data
      , int png_width
      , int png_height
#if defined(OS_CHROMEOS)
      , const std::string& user_email_text
      , const char* zipped_logs_data
      , int zipped_logs_length
      , const chromeos::system::LogDictionaryType* const sys_info
#endif
  );
  // Redirects the user to Google's phishing reporting page.
  static void ReportPhishing(TabContents* currentTab,
                             const std::string& phishing_url);
  // Maintains a single vector of bytes to store the last screenshot taken.
  static std::vector<unsigned char>* GetScreenshotPng();
  static void ClearScreenshotPng();
  static void SetScreenshotSize(const gfx::Rect& rect);
  static gfx::Rect& GetScreenshotSize();

  class PostCleanup;

 private:
  // Add a key value pair to the feedback object
  static void AddFeedbackData(
      userfeedback::ExternalExtensionSubmit* feedback_data,
      const std::string& key, const std::string& value);

  // Send the feedback report
  static void SendFeedback(Profile* profile, std::string* feedback_data,
                           int64 previous_delay);

#if defined(OS_CHROMEOS)
  static bool ValidFeedbackSize(const std::string& content);
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(BugReportUtil);
};

#endif  // CHROME_BROWSER_BUG_REPORT_UTIL_H_
