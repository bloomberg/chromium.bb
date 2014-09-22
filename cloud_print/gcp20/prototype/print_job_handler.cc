// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/print_job_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cloud_print/gcp20/prototype/gcp20_switches.h"

namespace {

const int kDraftExpirationSec = 10;
const int kLocalPrintJobExpirationSec = 20;
const int kErrorTimeoutSec = 30;

// Errors simulation constants:
const double kPaperJamProbability = 1.0;
const int kTooManyDraftsTimeout = 10;
const size_t kMaxDrafts = 5;

const base::FilePath::CharType kJobsPath[] = FILE_PATH_LITERAL("printjobs");

bool ValidateTicket(const std::string& ticket) {
  return true;
}

std::string GenerateId() {
  return base::StringToLowerASCII(base::GenerateGUID());
}

}  // namespace

struct PrintJobHandler::LocalPrintJobExtended {
  LocalPrintJobExtended(const LocalPrintJob& job, const std::string& ticket)
      : data(job),
        ticket(ticket),
        state(LocalPrintJob::STATE_DRAFT) {}
  LocalPrintJobExtended() : state(LocalPrintJob::STATE_DRAFT) {}
  ~LocalPrintJobExtended() {}

  LocalPrintJob data;
  std::string ticket;
  LocalPrintJob::State state;
  base::Time expiration;
};

struct PrintJobHandler::LocalPrintJobDraft {
  LocalPrintJobDraft() {}
  LocalPrintJobDraft(const std::string& ticket, const base::Time& expiration)
      : ticket(ticket),
        expiration(expiration) {}
  ~LocalPrintJobDraft() {}

  std::string ticket;
  base::Time expiration;
};

using base::StringPrintf;

PrintJobHandler::PrintJobHandler() {
}

PrintJobHandler::~PrintJobHandler() {
}

LocalPrintJob::CreateResult PrintJobHandler::CreatePrintJob(
    const std::string& ticket,
    std::string* job_id_out,
    // TODO(maksymb): Use base::TimeDelta for timeout values
    int* expires_in_out,
    // TODO(maksymb): Use base::TimeDelta for timeout values
    int* error_timeout_out,
    std::string* error_description) {
  if (!ValidateTicket(ticket))
    return LocalPrintJob::CREATE_INVALID_TICKET;

  // Let's simulate at least some errors just for testing.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulatePrintingErrors)) {
    if (base::RandDouble() <= kPaperJamProbability) {
      *error_description = "Paper jam, try again";
      return LocalPrintJob::CREATE_PRINTER_ERROR;
    }

    if (drafts.size() > kMaxDrafts) {  // Another simulation of error: business
      *error_timeout_out = kTooManyDraftsTimeout;
      return LocalPrintJob::CREATE_PRINTER_BUSY;
    }
  }

  std::string id = GenerateId();
  drafts[id] = LocalPrintJobDraft(
      ticket,
      base::Time::Now() + base::TimeDelta::FromSeconds(kDraftExpirationSec));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrintJobHandler::ForgetDraft, AsWeakPtr(), id),
      base::TimeDelta::FromSeconds(kDraftExpirationSec));

  *job_id_out = id;
  *expires_in_out = kDraftExpirationSec;
  return LocalPrintJob::CREATE_SUCCESS;
}

LocalPrintJob::SaveResult PrintJobHandler::SaveLocalPrintJob(
    const LocalPrintJob& job,
    std::string* job_id_out,
    int* expires_in_out,
    std::string* error_description_out,
    int* timeout_out) {
  std::string id;
  int expires_in;

  switch (CreatePrintJob(std::string(), &id, &expires_in,
                         timeout_out, error_description_out)) {
    case LocalPrintJob::CREATE_INVALID_TICKET:
      NOTREACHED();
      return LocalPrintJob::SAVE_SUCCESS;

    case LocalPrintJob::CREATE_PRINTER_BUSY:
      return LocalPrintJob::SAVE_PRINTER_BUSY;

    case LocalPrintJob::CREATE_PRINTER_ERROR:
      return LocalPrintJob::SAVE_PRINTER_ERROR;

    case LocalPrintJob::CREATE_SUCCESS:
      *job_id_out = id;
      return CompleteLocalPrintJob(job, id, expires_in_out,
                                   error_description_out, timeout_out);

    default:
      NOTREACHED();
      return LocalPrintJob::SAVE_SUCCESS;
  }
}

LocalPrintJob::SaveResult PrintJobHandler::CompleteLocalPrintJob(
    const LocalPrintJob& job,
    const std::string& job_id,
    int* expires_in_out,
    std::string* error_description_out,
    int* timeout_out) {
  if (!drafts.count(job_id)) {
    *timeout_out = kErrorTimeoutSec;
    return LocalPrintJob::SAVE_INVALID_PRINT_JOB;
  }

  std::string file_extension;
  // TODO(maksymb): Gather together this type checking with Printer
  // supported types in kCdd.
  if (job.content_type == "application/pdf") {
    file_extension = "pdf";
  } else if (job.content_type == "image/pwg-raster") {
    file_extension = "pwg";
  } else if (job.content_type == "image/jpeg") {
    file_extension = "jpg";
  } else {
    error_description_out->clear();
    return LocalPrintJob::SAVE_INVALID_DOCUMENT_TYPE;
  }
  CompleteDraft(job_id, job);
  std::map<std::string, LocalPrintJobExtended>::iterator current_job =
      jobs.find(job_id);

  if (!SavePrintJob(current_job->second.data.content,
                    current_job->second.ticket,
                    base::Time::Now(),
                    StringPrintf("%s", job_id.c_str()),
                    current_job->second.data.job_name, file_extension)) {
    SetJobState(job_id, LocalPrintJob::STATE_ABORTED);
    *error_description_out = "IO error";
    return LocalPrintJob::SAVE_PRINTER_ERROR;
  }

  SetJobState(job_id, LocalPrintJob::STATE_DONE);
  *expires_in_out = static_cast<int>(GetJobExpiration(job_id).InSeconds());
  return LocalPrintJob::SAVE_SUCCESS;
}

bool PrintJobHandler::GetJobState(const std::string& id,
                                  LocalPrintJob::Info* info_out) {
  using base::Time;

  std::map<std::string, LocalPrintJobDraft>::iterator draft = drafts.find(id);
  if (draft != drafts.end()) {
    info_out->state = LocalPrintJob::STATE_DRAFT;
    info_out->expires_in =
        static_cast<int>((draft->second.expiration - Time::Now()).InSeconds());
    return true;
  }

  std::map<std::string, LocalPrintJobExtended>::iterator job = jobs.find(id);
  if (job != jobs.end()) {
    info_out->state = job->second.state;
    info_out->expires_in = static_cast<int>(GetJobExpiration(id).InSeconds());
    return true;
  }
  return false;
}

bool PrintJobHandler::SavePrintJob(const std::string& content,
                                   const std::string& ticket,
                                   const base::Time& create_time,
                                   const std::string& id,
                                   const std::string& title,
                                   const std::string& file_extension) {
  VLOG(1) << "Printing printjob: \"" + title + "\"";
  base::FilePath directory(kJobsPath);

  if (!base::DirectoryExists(directory) &&
      !base::CreateDirectory(directory)) {
    return false;
  }

  base::Time::Exploded create_time_exploded;
  create_time.UTCExplode(&create_time_exploded);
  base::FilePath::StringType job_prefix =
      StringPrintf(FILE_PATH_LITERAL("%.4d%.2d%.2d-%.2d%.2d%.2d_"),
                   create_time_exploded.year,
                   create_time_exploded.month,
                   create_time_exploded.day_of_month,
                   create_time_exploded.hour,
                   create_time_exploded.minute,
                   create_time_exploded.second);
  if (!base::CreateTemporaryDirInDir(directory, job_prefix, &directory)) {
    LOG(WARNING) << "Cannot create directory for " << job_prefix;
    return false;
  }

  int written = base::WriteFile(directory.AppendASCII("ticket.xml"),
                                ticket.data(),
                                static_cast<int>(ticket.size()));
  if (static_cast<size_t>(written) != ticket.size()) {
    LOG(WARNING) << "Cannot save ticket.";
    return false;
  }

  written = base::WriteFile(
      directory.AppendASCII("data." + file_extension),
      content.data(), static_cast<int>(content.size()));
  if (static_cast<size_t>(written) != content.size()) {
    LOG(WARNING) << "Cannot save data.";
    return false;
  }

  VLOG(0) << "Job saved at " << directory.value();
  return true;
}

void PrintJobHandler::SetJobState(const std::string& id,
                                  LocalPrintJob::State state) {
  DCHECK(!drafts.count(id)) << "Draft should be completed at first";

  std::map<std::string, LocalPrintJobExtended>::iterator job = jobs.find(id);
  DCHECK(job != jobs.end());
  job->second.state = state;
  switch (state) {
    case LocalPrintJob::STATE_DRAFT:
      NOTREACHED();
      break;
    case LocalPrintJob::STATE_ABORTED:
    case LocalPrintJob::STATE_DONE:
      job->second.expiration =
          base::Time::Now() +
          base::TimeDelta::FromSeconds(kLocalPrintJobExpirationSec);
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&PrintJobHandler::ForgetLocalJob, AsWeakPtr(), id),
          base::TimeDelta::FromSeconds(kLocalPrintJobExpirationSec));
      break;
    default:
      NOTREACHED();
  }
}

void PrintJobHandler::CompleteDraft(const std::string& id,
                                    const LocalPrintJob& job) {
  std::map<std::string, LocalPrintJobDraft>::iterator draft = drafts.find(id);
  if (draft != drafts.end()) {
    jobs[id] = LocalPrintJobExtended(job, draft->second.ticket);
    drafts.erase(draft);
  }
}

// TODO(maksymb): Use base::Time for expiration
base::TimeDelta PrintJobHandler::GetJobExpiration(const std::string& id) const {
  DCHECK(jobs.count(id));
  base::Time expiration = jobs.at(id).expiration;
  if (expiration.is_null())
    return base::TimeDelta::FromSeconds(kLocalPrintJobExpirationSec);
  return expiration - base::Time::Now();
}

void PrintJobHandler::ForgetDraft(const std::string& id) {
  drafts.erase(id);
}

void PrintJobHandler::ForgetLocalJob(const std::string& id) {
  jobs.erase(id);
}
