// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/print_job_handler.h"

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace {

const int kLocalPrintJobInitExpiration = 5*60;  // in seconds

const base::FilePath::CharType kJobsPath[] = FILE_PATH_LITERAL("printjobs");

}  // namespace

using base::StringPrintf;

PrintJobHandler::PrintJobHandler() {
}

PrintJobHandler::~PrintJobHandler() {
}

LocalPrintJob::SaveResult PrintJobHandler::SaveLocalPrintJob(
    const LocalPrintJob& job,
    std::string* job_id,
    std::string* error_description,
    int* timeout) {
  std::string suffix = StringPrintf("%s:%s",
                                    job.user_name.c_str(),
                                    job.client_name.c_str());
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
    error_description->clear();
    return LocalPrintJob::SAVE_INVALID_DOCUMENT_TYPE;
  }

  std::string id = StringToLowerASCII(base::GenerateGUID());
  if (!SavePrintJob(job.content, std::string(),
                   base::Time::Now(),
                   StringPrintf("%s", id.c_str()),
                   suffix, job.job_name, file_extension)) {
    *error_description = "IO error";
    return LocalPrintJob::SAVE_PRINTER_ERROR;
  }

  *job_id = id;
  return LocalPrintJob::SAVE_SUCCESS;
}

bool PrintJobHandler::SavePrintJob(const std::string& content,
                                   const std::string& ticket,
                                   const base::Time& create_time,
                                   const std::string& id,
                                   const std::string& job_name_suffix,
                                   // suffix is not extension
                                   // it may be used to mark local printer jobs
                                   const std::string& title,
                                   const std::string& file_extension) {
  VLOG(1) << "Printing printjob: \"" + title + "\"";
  base::FilePath directory(kJobsPath);

  if (!base::DirectoryExists(directory) &&
      !file_util::CreateDirectory(directory)) {
    LOG(WARNING) << "Cannot create directory: " << directory.value();
    return false;
  }

  base::Time::Exploded create_time_exploded;
  create_time.UTCExplode(&create_time_exploded);
  std::string job_name =
      StringPrintf("%.4d%.2d%.2d-%.2d:%.2d:%.2d-%.3dms.%s",
          create_time_exploded.year,
          create_time_exploded.month,
          create_time_exploded.day_of_month,
          create_time_exploded.hour,
          create_time_exploded.minute,
          create_time_exploded.second,
          create_time_exploded.millisecond,
          id.c_str());
  if (!job_name_suffix.empty())
    job_name += "." + job_name_suffix;
  directory = directory.AppendASCII(job_name);

  if (!base::DirectoryExists(directory) &&
      !file_util::CreateDirectory(directory)) {
    LOG(WARNING) << "Cannot create directory: " << directory.value();
    return false;
  }

  int written = file_util::WriteFile(directory.AppendASCII("ticket.xml"),
                                     ticket.data(),
                                     static_cast<int>(ticket.size()));
  if (static_cast<size_t>(written) != ticket.size()) {
    LOG(WARNING) << "Cannot save ticket.";
    return false;
  }

  written = file_util::WriteFile(
      directory.AppendASCII("data." + file_extension),
      content.data(), static_cast<int>(content.size()));
  if (static_cast<size_t>(written) != content.size()) {
    LOG(WARNING) << "Cannot save data.";
    return false;
  }

  LOG(INFO) << "Saved printjob: " << job_name << ": " << title;
  return true;
}

