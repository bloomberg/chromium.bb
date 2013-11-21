// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cloud_print/gcp20/prototype/local_print_job.h"

namespace base {

class DictionaryValue;
class Time;

}  // namespace base

class PrintJobHandler : public base::SupportsWeakPtr<PrintJobHandler> {
 public:
  PrintJobHandler();
  ~PrintJobHandler();

  // Creates printer job draft
  LocalPrintJob::CreateResult CreatePrintJob(
      const std::string& ticket,
      std::string* job_id_out,
      int* expires_in_out,
      int* error_timeout_out,
      std::string* error_description_out);

  // Creates printer job with empty ticket and "prints" it
  LocalPrintJob::SaveResult SaveLocalPrintJob(
      const LocalPrintJob& job,
      std::string* job_id_out,
      int* expires_in_out,
      std::string* error_description_out,
      int* timeout_out);

  // Completes printer job from draft
  LocalPrintJob::SaveResult CompleteLocalPrintJob(
      const LocalPrintJob& job,
      const std::string& job_id,
      int* expires_in_out,
      std::string* error_description_out,
      int* timeout_out);

  // Gives info about job
  bool GetJobState(const std::string& id, LocalPrintJob::Info* info_out);

  // Saving print job directly to drive
  bool SavePrintJob(const std::string& content,
                    const std::string& ticket,
                    const base::Time& create_time,
                    const std::string& id,
                    const std::string& title,
                    const std::string& file_extension);

 private:
  // Contains ticket info and job info together
  struct LocalPrintJobExtended;

  // Contains job ticket
  struct LocalPrintJobDraft;

  // Contains all unexpired drafts
  std::map<std::string, LocalPrintJobDraft> drafts;  // id -> draft

  // Contains all unexpired jobs
  std::map<std::string, LocalPrintJobExtended> jobs;  // id -> printjob

  // Changes job state and creates timeouts to delete old jobs from memory
  void SetJobState(const std::string& id, LocalPrintJob::State);

  // Moves draft to jobs
  void CompleteDraft(const std::string& id, const LocalPrintJob& job);

  // Calculates expiration for job
  // TODO(maksymb): Use base::Time for expiration
  base::TimeDelta GetJobExpiration(const std::string& id) const;

  // Erases draft from memory
  void ForgetDraft(const std::string& id);

  // Erases job from memory
  void ForgetLocalJob(const std::string& id);

  DISALLOW_COPY_AND_ASSIGN(PrintJobHandler);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_

