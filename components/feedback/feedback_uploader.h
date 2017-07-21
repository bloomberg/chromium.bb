// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_

#include <queue>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace feedback {

class FeedbackReport;

// FeedbackUploader is used to add a feedback report to the queue of reports
// being uploaded. In case uploading a report fails, it is written to disk and
// tried again when it's turn comes up next in the queue.
class FeedbackUploader : public base::SupportsWeakPtr<FeedbackUploader> {
 public:
  FeedbackUploader(const base::FilePath& path,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~FeedbackUploader();

  static void SetMinimumRetryDelayForTesting(base::TimeDelta delay);

  // Queues a report for uploading.
  void QueueReport(const std::string& data);

  bool QueueEmpty() const { return reports_queue_.empty(); }

  const base::FilePath& feedback_reports_path() const {
    return feedback_reports_path_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  base::TimeDelta retry_delay() const { return retry_delay_; }

  const GURL& feedback_post_url() const { return feedback_post_url_; }

 protected:
  // Invoked when a feedback report upload succeeds. It will reset the
  // |retry_delay_| to its minimum value and schedules the next report upload if
  // any.
  void OnReportUploadSuccess();

  // Invoked when |report| fails to upload. It will double the |retry_delay_|
  // and reenqueue |report| with the new delay. All subsequent retries will keep
  // increasing the delay until a successful upload is encountered.
  void OnReportUploadFailure(scoped_refptr<FeedbackReport> report);

 private:
  friend class FeedbackUploaderTest;

  struct ReportsUploadTimeComparator {
    bool operator()(const scoped_refptr<FeedbackReport>& a,
                    const scoped_refptr<FeedbackReport>& b) const;
  };

  // Dispatches the report to be uploaded.
  virtual void DispatchReport(scoped_refptr<FeedbackReport> report) = 0;

  // Update our timer for uploading the next report.
  void UpdateUploadTimer();

  void QueueReportWithDelay(const std::string& data, base::TimeDelta delay);

  const base::FilePath feedback_reports_path_;

  // Timer to upload the next report at.
  base::OneShotTimer upload_timer_;

  // See comment of |FeedbackUploaderFactory::task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Priority queue of reports prioritized by the time the report is supposed
  // to be uploaded at.
  std::priority_queue<scoped_refptr<FeedbackReport>,
                      std::vector<scoped_refptr<FeedbackReport>>,
                      ReportsUploadTimeComparator>
      reports_queue_;

  base::TimeDelta retry_delay_;
  const GURL feedback_post_url_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploader);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_H_
