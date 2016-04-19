// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_QUEUE_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_QUEUE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"

namespace base {
class DictionaryValue;
}

namespace cloud_print {

struct JobDetails {
  JobDetails();
  JobDetails(const JobDetails& other);
  ~JobDetails();
  void Clear();
  static bool ordering(const JobDetails& first, const JobDetails& second);

  std::string job_id_;
  std::string job_title_;
  std::string job_owner_;

  std::string print_ticket_url_;
  std::string print_data_url_;

  std::string print_ticket_;
  std::string print_ticket_mime_type_;
  base::FilePath print_data_file_path_;
  std::string print_data_mime_type_;

  std::vector<std::string> tags_;

  base::TimeDelta time_remaining_;
};

// class containing logic for print job backoff

class PrinterJobQueueHandler {
 public:
  class TimeProvider {
   public:
    virtual base::Time GetNow() = 0;
    virtual ~TimeProvider() {}
  };

  // PrinterJobQueueHandler takes ownership of |time_provider| and is
  // responsible for deleting it.
  explicit PrinterJobQueueHandler(TimeProvider* time_provider);
  PrinterJobQueueHandler();
  ~PrinterJobQueueHandler();

  // jobs will be filled with details of all jobs in the queue, sorted by time
  // until they are ready to print, lowest to highest. Jobs that are ready to
  // print will have a time_remaining_ of 0.
  void GetJobsFromQueue(const base::DictionaryValue* json_data,
                        std::vector<JobDetails>* jobs);

  // Marks a job fetch as failed. Returns "true" if the job will be retried.
  bool JobFetchFailed(const std::string& job_id);

  void JobDone(const std::string& job_id);

 private:
  std::unique_ptr<TimeProvider> time_provider_;

  struct FailedJobMetadata {
    int retries_;
    base::Time last_retry_;
  };

  typedef std::map<std::string, FailedJobMetadata> FailedJobMap;
  typedef std::pair<std::string, FailedJobMetadata> FailedJobPair;

  FailedJobMap failed_job_map_;

  void ConstructJobDetailsFromJson(const base::DictionaryValue* json_data,
                                   JobDetails* details_obj);
  base::TimeDelta ComputeBackoffTime(const std::string& job_id);

  DISALLOW_COPY_AND_ASSIGN(PrinterJobQueueHandler);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_QUEUE_HANDLER_H_


