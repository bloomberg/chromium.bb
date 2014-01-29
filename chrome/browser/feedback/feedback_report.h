// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_REPORT_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_REPORT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace content {
class BrowserContext;
}

namespace feedback {

typedef base::Callback<void(const std::string&)> QueueCallback;

// This class holds a feedback report. Once a report is created, a disk backup
// for it is created automatically. This backup is deleted once this object
// dies.
class FeedbackReport : public base::RefCountedThreadSafe<FeedbackReport> {
 public:
  FeedbackReport(content::BrowserContext* context,
                 const base::Time& upload_at,
                 const std::string& data);

  // Stops the disk write of the report and deletes the report file if already
  // written.
  void DeleteReportOnDisk();

  base::Time upload_at() { return upload_at_; }
  const std::string& data() { return data_; }

  // Loads the reports still on disk and queues then using the given callback.
  // This call blocks on the file reads.
  static void LoadReportsAndQueue(content::BrowserContext* context,
                                  QueueCallback callback);

 private:
  friend class base::RefCountedThreadSafe<FeedbackReport>;
  virtual ~FeedbackReport();

  void WriteReportOnBlockingPool();

  // Name of the file corresponding to this report.
  base::FilePath file_;

  content::BrowserContext* context_;
  base::Time upload_at_;  // Upload this report at or after this time.
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackReport);
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_REPORT_H_
