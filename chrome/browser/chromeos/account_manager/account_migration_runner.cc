// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_migration_runner.h"

#include <string>
#include <utility>

#include "base/bind.h"

namespace chromeos {

AccountMigrationRunner::Step::Step(const std::string& id) : id_(id) {}

AccountMigrationRunner::Step::~Step() = default;

const std::string& AccountMigrationRunner::Step::GetId() const {
  return id_;
}

AccountMigrationRunner::AccountMigrationRunner() : weak_factory_(this) {}

AccountMigrationRunner::~AccountMigrationRunner() = default;

AccountMigrationRunner::Status AccountMigrationRunner::GetStatus() const {
  return status_;
}

void AccountMigrationRunner::AddStep(std::unique_ptr<Step> step) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |Step|s cannot be added after migration has begun.
  DCHECK_EQ(Status::kNotStarted, status_);

  steps_.emplace(std::move(step));
}

void AccountMigrationRunner::Run(OnMigrationDone callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_ != Status::kNotStarted) {
    // Ignore calls to |Run| after migration has begun.
    return;
  }
  status_ = Status::kRunning;
  callback_ = std::move(callback);

  RunNextStep();
}

void AccountMigrationRunner::RunNextStep() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (steps_.empty()) {
    FinishWithSuccess();
    return;
  }

  current_step_ = std::move(steps_.front());
  steps_.pop();

  current_step_->Run(base::BindOnce(&AccountMigrationRunner::OnStepCompleted,
                                    weak_factory_.GetWeakPtr()));
}

void AccountMigrationRunner::OnStepCompleted(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result) {
    RunNextStep();
  } else {
    FinishWithFailure();
  }
}

void AccountMigrationRunner::FinishWithSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  status_ = Status::kSuccess;
  DCHECK(callback_);
  std::move(callback_).Run(MigrationResult{Status::kSuccess /* final_status */,
                                           std::string() /* failed_step */});
}

void AccountMigrationRunner::FinishWithFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  status_ = Status::kFailure;
  DCHECK(callback_);
  std::move(callback_).Run(
      MigrationResult{Status::kFailure /* final_status */,
                      current_step_->GetId() /* failed_step */});
}

}  // namespace chromeos
