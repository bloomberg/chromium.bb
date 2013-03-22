// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/webui/screenshot_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system_logs/system_logs_fetcher.h"
#endif

class Profile;

class FeedbackData : public base::RefCountedThreadSafe<FeedbackData> {
 public:
  FeedbackData();

  // Called once we've update all the data from the feedback page.
  void FeedbackPageDataComplete();

#if defined(OS_CHROMEOS)
  // Called once we have read our system logs.
  void CompressSyslogs(scoped_ptr<chromeos::SystemLogsResponse> sys_info);

  // Called once we have read and compressed our system logs.
  void SyslogsComplete(scoped_ptr<chromeos::SystemLogsResponse> sys_info,
                       scoped_ptr<std::string> compressed_logs);

  // Called once we have read our attached file.
  void ReadFileComplete();

  // Starts the collection of our system logs. SyslogsComplete is called once
  // the collection is done.
  void StartSyslogsCollection();
#endif

  // Returns true if we've complete collection of data from all our
  // data sources. At this time this involves the system logs, the attached
  // file (if needed to be read of the disk) and the rest of the data from
  // the feedback page.
  bool DataCollectionComplete();

  // Sends the feedback report if we have all our data complete.
  void SendReport();

  // Starts reading the file to attach to this report into the string
  // file_data. ReadFileComplete is called once this is done.
  void StartReadFile(const std::string filename, const std::string* file_data);

  // Getters
  Profile* profile() const { return profile_; }
  const std::string& category_tag() const { return category_tag_; }
  const std::string& page_url() const { return page_url_; }
  const std::string& description() const { return description_; }
  const std::string& user_email() const { return user_email_; }
  ScreenshotDataPtr image() const { return image_; }
#if defined(OS_CHROMEOS)
  chromeos::SystemLogsResponse* sys_info() const {
    return send_sys_info_ ? sys_info_.get() : NULL;
  }
  const std::string timestamp() const { return timestamp_; }
  const std::string attached_filename() const { return attached_filename_; }
  std::string* attached_filedata() const { return attached_filedata_.get(); }
  std::string* compressed_logs() const { return compressed_logs_.get(); }
#endif

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
  void set_image(ScreenshotDataPtr image) { image_ = image; }
#if defined(OS_CHROMEOS)
  void set_send_sys_info(bool send_sys_info) { send_sys_info_ = send_sys_info; }
  void set_timestamp(const std::string& timestamp) {
    timestamp_ = timestamp;
  }
  void set_attached_filename(const std::string& attached_filename) {
    attached_filename_ = attached_filename;
  }
  void set_attached_filedata(scoped_ptr<std::string> attached_filedata) {
    attached_filedata_ = attached_filedata.Pass();
  }
#endif

 private:
  friend class base::RefCountedThreadSafe<FeedbackData>;

  virtual ~FeedbackData();

#if defined(OS_CHROMEOS)
  void ReadAttachedFile(const base::FilePath& from);
#endif

  Profile* profile_;

  std::string category_tag_;
  std::string page_url_;
  std::string description_;
  std::string user_email_;
  ScreenshotDataPtr image_;

#if defined(OS_CHROMEOS)
  // Chromeos specific values for SendReport. Will be deleted in
  // feedback_util::SendReport once consumed or in SyslogsComplete if
  // we don't send the logs with the report.
  scoped_ptr<chromeos::SystemLogsResponse> sys_info_;

  std::string timestamp_;
  std::string attached_filename_;
  scoped_ptr<std::string> attached_filedata_;
  scoped_ptr<std::string> compressed_logs_;

  // Flag to indicate whether or not we should send the system information
  // gathered with the report or not.
  bool send_sys_info_;

  // Flags to indicate if various pieces of data needed for the report have
  // been collected yet or are still pending collection.
  bool read_attached_file_complete_;
  bool syslogs_collection_complete_;
#endif
  bool feedback_page_data_complete_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackData);
};

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_DATA_H_
