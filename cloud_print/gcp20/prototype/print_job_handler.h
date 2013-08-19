// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "cloud_print/gcp20/prototype/local_print_job.h"

namespace base {

class DictionaryValue;
class Time;

}  // namespace base

class PrintJobHandler {
 public:
  PrintJobHandler();
  ~PrintJobHandler();

  LocalPrintJob::CreateResult CreatePrintJob(std::string* job_id,
                                             int* expires_in);
  LocalPrintJob::SaveResult SaveLocalPrintJob(const LocalPrintJob& job,
                                              std::string* job_id,
                                              std::string* error_description,
                                              int* timeout);

  bool SavePrintJob(const std::string& content,
                    const std::string& ticket,
                    const base::Time& create_time,
                    const std::string& id,
                    const std::string& job_name_suffix,
                    const std::string& title,
                    const std::string& file_extension);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintJobHandler);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_

