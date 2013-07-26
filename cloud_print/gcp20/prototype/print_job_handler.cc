// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/print_job_handler.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace {

const base::FilePath::CharType kJobsPath[] = FILE_PATH_LITERAL("printjobs");

}  // namespace

PrintJobHandler::PrintJobHandler() {
}

PrintJobHandler::~PrintJobHandler() {
}

bool PrintJobHandler::SavePrintJob(const std::string& data,
                                   const std::string& ticket,
                                   const std::string& job_name,
                                   const std::string& title) {
  VLOG(1) << "Printing printjob: \"" + title + "\"";
  base::FilePath directory(kJobsPath);

  using file_util::CreateDirectory;

  if (!base::DirectoryExists(directory) && !CreateDirectory(directory)) {
    LOG(WARNING) << "Cannot create directory: " << directory.value();
    return false;
  }

  directory = directory.AppendASCII(job_name);

  if (!base::DirectoryExists(directory) && !CreateDirectory(directory)) {
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

  written = file_util::WriteFile(directory.AppendASCII("data.pdf"),
                                 data.data(),
                                 static_cast<int>(data.size()));
  if (static_cast<size_t>(written) != data.size()) {
    LOG(WARNING) << "Cannot save data.";
    return false;
  }

  LOG(INFO) << "Saved printjob: " << job_name;
  return true;
}

