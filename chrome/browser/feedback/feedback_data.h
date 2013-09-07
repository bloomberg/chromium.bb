// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}
class Profile;

class FeedbackData : public base::RefCountedThreadSafe<FeedbackData> {
 public:
  typedef std::map<std::string, std::string> SystemLogsMap;

  static const char kScreensizeHeightKey[];
  static const char kScreensizeWidthKey[];

  FeedbackData();

  // Called once we've updated all the data from the feedback page.
  void OnFeedbackPageDataComplete();

  // Sets the system information for this instance and kicks off its
  // compression.
  void SetAndCompressSystemInfo(scoped_ptr<SystemLogsMap> sys_info);

  // Called once we have compressed our system logs.
  void OnCompressLogsComplete(scoped_ptr<std::string> compressed_logs);

  // Returns true if we've completed all the tasks needed before we can send
  // feedback - at this time this is includes getting the feedback page data
  // and compressing the system logs.
  bool IsDataComplete();

  // Sends the feedback report if we have all our data complete.
  void SendReport();

  // Getters
  Profile* profile() const { return profile_; }
  const std::string& category_tag() const { return category_tag_; }
  const std::string& page_url() const { return page_url_; }
  const std::string& description() const { return description_; }
  const std::string& user_email() const { return user_email_; }
  std::string* image() const { return image_.get(); }
  const std::string attached_filename() const { return attached_filename_; }
  const GURL attached_file_url() const { return attached_file_url_; }
  std::string* attached_filedata() const { return attached_filedata_.get(); }
  const GURL screenshot_url() const { return screenshot_url_; }
  SystemLogsMap* sys_info() const { return sys_info_.get(); }
  std::string* compressed_logs() const { return compressed_logs_.get(); }

  // Setters
  void set_profile(Profile* profile) { profile_ = profile; }
  void set_category_tag(const std::string& category_tag) {
    category_tag_ = category_tag;
  }
  void set_page_url(const std::string& page_url) { page_url_ = page_url; }
  void set_description(const std::string& description) {
    description_ = description;
  }
  void set_user_email(const std::string& user_email) {
    user_email_ = user_email;
  }
  void set_image(scoped_ptr<std::string> image) { image_ = image.Pass(); }
  void set_attached_filename(const std::string& attached_filename) {
    attached_filename_ = attached_filename;
  }
  void set_attached_filedata(scoped_ptr<std::string> attached_filedata) {
    attached_filedata_ = attached_filedata.Pass();
  }
  void set_attached_file_url(const GURL& url) { attached_file_url_ = url; }
  void set_screenshot_url(const GURL& url) { screenshot_url_ = url; }

 private:
  friend class base::RefCountedThreadSafe<FeedbackData>;

  virtual ~FeedbackData();

  Profile* profile_;

  std::string category_tag_;
  std::string page_url_;
  std::string description_;
  std::string user_email_;
  scoped_ptr<std::string> image_;
  std::string attached_filename_;
  scoped_ptr<std::string> attached_filedata_;

  GURL attached_file_url_;
  GURL screenshot_url_;

  scoped_ptr<SystemLogsMap> sys_info_;
  scoped_ptr<std::string> compressed_logs_;

  bool feedback_page_data_complete_;
  bool syslogs_compression_complete_;

  bool report_sent_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackData);
};

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_
