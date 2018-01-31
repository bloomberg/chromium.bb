// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/password_manager_exporter.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"

namespace {

// A wrapper for |write_function|, which can be bound and keep a copy of its
// data on the closure.
bool Write(int (*write_function)(const base::FilePath& filename,
                                 const char* data,
                                 int size),
           const base::FilePath& destination,
           const std::string& serialised) {
  int written =
      write_function(destination, serialised.c_str(), serialised.size());
  return written == static_cast<int>(serialised.size());
}

}  // namespace

namespace password_manager {

PasswordManagerExporter::PasswordManagerExporter(
    password_manager::CredentialProviderInterface*
        credential_provider_interface,
    ProgressCallback on_progress)
    : credential_provider_interface_(credential_provider_interface),
      on_progress_(std::move(on_progress)),
      last_progress_status_(ExportProgressStatus::NOT_STARTED),
      write_function_(&base::WriteFile),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock()})),
      weak_factory_(this) {}

PasswordManagerExporter::~PasswordManagerExporter() {}

void PasswordManagerExporter::PreparePasswordsForExport() {
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list =
      credential_provider_interface_->GetAllPasswords();
  size_t password_list_size = password_list.size();

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&password_manager::PasswordCSVWriter::SerializePasswords,
                     base::Passed(std::move(password_list))),
      base::BindOnce(&PasswordManagerExporter::SetSerialisedPasswordList,
                     weak_factory_.GetWeakPtr(), password_list_size));
}

void PasswordManagerExporter::SetDestination(
    const base::FilePath& destination) {
  destination_ = destination;

  if (IsReadyForExport())
    Export();

  OnProgress(ExportProgressStatus::IN_PROGRESS, std::string());
}

void PasswordManagerExporter::SetSerialisedPasswordList(
    size_t count,
    const std::string& serialised) {
  serialised_password_list_ = serialised;
  password_count_ = count;

  if (IsReadyForExport())
    Export();
}

void PasswordManagerExporter::Cancel() {
  // Tasks which had their pointers invalidated won't run.
  weak_factory_.InvalidateWeakPtrs();

  destination_.clear();
  serialised_password_list_.clear();
  password_count_ = 0;

  OnProgress(ExportProgressStatus::FAILED_CANCELLED, std::string());
}

password_manager::ExportProgressStatus
PasswordManagerExporter::GetProgressStatus() {
  return last_progress_status_;
}

void PasswordManagerExporter::SetWriteForTesting(
    int (*write_function)(const base::FilePath& filename,
                          const char* data,
                          int size)) {
  write_function_ = write_function;
}

bool PasswordManagerExporter::IsReadyForExport() {
  return !destination_.empty() && !serialised_password_list_.empty();
}

void PasswordManagerExporter::Export() {
  base::FilePath destination_copy_(destination_);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(::Write, write_function_, std::move(destination_copy_),
                     std::move(serialised_password_list_)),
      base::BindOnce(&PasswordManagerExporter::OnPasswordsExported,
                     weak_factory_.GetWeakPtr(), std::move(destination_),
                     password_count_));

  destination_.clear();
  serialised_password_list_.clear();
  password_count_ = 0;
}

void PasswordManagerExporter::OnPasswordsExported(
    const base::FilePath& destination,
    int count,
    bool success) {
  if (success) {
    UMA_HISTOGRAM_COUNTS("PasswordManager.ExportedPasswordsPerUserInCSV",
                         count);
    OnProgress(ExportProgressStatus::SUCCEEDED, std::string());
  } else {
    OnProgress(ExportProgressStatus::FAILED_WRITE_FAILED,
               destination.DirName().BaseName().AsUTF8Unsafe());
  }
}

void PasswordManagerExporter::OnProgress(
    password_manager::ExportProgressStatus status,
    const std::string& folder) {
  last_progress_status_ = status;
  on_progress_.Run(status, folder);
}

}  // namespace password_manager
