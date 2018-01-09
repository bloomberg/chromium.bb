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

std::vector<std::unique_ptr<autofill::PasswordForm>> CopyOf(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;
  for (const auto& form : password_list) {
    ret_val.push_back(std::make_unique<autofill::PasswordForm>(*form));
  }
  return ret_val;
}

}  // namespace

namespace password_manager {

PasswordManagerExporter::PasswordManagerExporter(
    password_manager::CredentialProviderInterface*
        credential_provider_interface)
    : credential_provider_interface_(credential_provider_interface),
      write_function_(&base::WriteFile),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock()})),
      weak_factory_(this) {}

PasswordManagerExporter::~PasswordManagerExporter() {}

void PasswordManagerExporter::PreparePasswordsForExport() {
  password_list_ = credential_provider_interface_->GetAllPasswords();

  if (IsReadyForExport())
    Export();
}

void PasswordManagerExporter::SetDestination(
    const base::FilePath& destination) {
  destination_ = destination;

  if (IsReadyForExport())
    Export();
}

void PasswordManagerExporter::Cancel() {
  // Tasks which had their pointers invalidated won't run.
  weak_factory_.InvalidateWeakPtrs();

  destination_.clear();
  password_list_.clear();
}

void PasswordManagerExporter::SetWriteForTesting(
    int (*write_function)(const base::FilePath& filename,
                          const char* data,
                          int size)) {
  write_function_ = write_function;
}

bool PasswordManagerExporter::IsReadyForExport() {
  return !destination_.empty() && !password_list_.empty();
}

void PasswordManagerExporter::Export() {
  UMA_HISTOGRAM_COUNTS("PasswordManager.ExportedPasswordsPerUserInCSV",
                       password_list_.size());

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&password_manager::PasswordCSVWriter::SerializePasswords,
                     base::Passed(CopyOf(password_list_))),
      base::BindOnce(&PasswordManagerExporter::OnPasswordsSerialised,
                     weak_factory_.GetWeakPtr(),
                     base::Passed(std::move(destination_))));

  password_list_.clear();
  destination_.clear();
}

void PasswordManagerExporter::OnPasswordsSerialised(
    base::FilePath destination,
    const std::string& serialised) {
  write_function_(destination, serialised.c_str(), serialised.size());
}

}  // namespace password_manager
