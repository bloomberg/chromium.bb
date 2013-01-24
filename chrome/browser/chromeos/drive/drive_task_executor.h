// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_TASK_EXECUTOR_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_TASK_EXECUTOR_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace drive {

class DriveEntryProto;

// This class implements an "executor" class that will execute tasks for
// third party Drive apps that store data in Drive itself.  To do that, it
// needs to find the file resource IDs and pass them to a server-side function
// that will authorize the app to open the given document and return a URL
// for opening the document in that app directly.
class DriveTaskExecutor : public file_handler_util::FileTaskExecutor {
 public:
  // FileTaskExecutor overrides
  virtual bool ExecuteAndNotify(
      const std::vector<fileapi::FileSystemURL>& file_urls,
      const file_handler_util::FileTaskFinishedCallback& done) OVERRIDE;

 private:
  // FileTaskExecutor is the only class allowed to create one.
  friend class file_handler_util::FileTaskExecutor;

  DriveTaskExecutor(Profile* profile,
                    const std::string& app_id,
                    const std::string& action_id);
  virtual ~DriveTaskExecutor();

  void OnFileEntryFetched(DriveFileError error,
                          scoped_ptr<DriveEntryProto> entry_proto);
  void OnAppAuthorized(const std::string& resource_id,
                       google_apis::GDataErrorCode error,
                       const GURL& open_link);

  // Calls |done_| with |success| status.
  void Done(bool success);

  const GURL source_url_;
  const std::string action_id_;
  int current_index_;
  file_handler_util::FileTaskFinishedCallback done_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_TASK_EXECUTOR_H_
