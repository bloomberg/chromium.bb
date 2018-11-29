// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MIGRATION_RUNNER_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MIGRATION_RUNNER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace chromeos {

// A utility class to run account migrations for |chromeos::AccountManager|. It
// enables the specification of a series of async migration |Step|s in a
// declarative style (see the test cases for usage examples). If any of these
// |Step|s fail, the entire migration fails, without running subsequent steps.
class AccountMigrationRunner {
 public:
  enum class Status {
    // Migration has not started yet.
    kNotStarted,
    // Migration is in progress.
    kRunning,
    // All migration steps completed successfully.
    kSuccess,
    // Migration failed at some |Step|.
    kFailure,
  };

  struct MigrationResult {
    // Final status of migration.
    Status final_status;
    // If |final_status| is |FAILURE|, this field will contain the id of the
    // step, that caused migration to fail.
    std::string failed_step_id;
  };

  // Type alias for the callback called at the end of a migration run.
  using OnMigrationDone = base::OnceCallback<void(const MigrationResult&)>;

  // Abstract base class for a migration step.
  class Step {
   public:
    // |id| is an identifier for this step. This should be unique across |Step|s
    // added to |AccountMigrationRunner::AddStep| but maintaining this
    // uniqueness is a responsibility of the callers and not enforced by
    // |AccountMigrationRunner|.
    explicit Step(const std::string& id);

    virtual ~Step();

    // Runs the migration step and calls |callback| with the result (|true| for
    // success and |false| for failure) of the migration step.
    // Note that |AccountMigrationRunner| does not guarantee anything about the
    // lifetime of a |Step| once it has been added via
    // |AccountMigrationRunner::AddStep|.
    virtual void Run(base::OnceCallback<void(bool)> callback) = 0;

    // Gets this |Step|'s identifier.
    const std::string& GetId() const;

   private:
    const std::string id_;
    DISALLOW_COPY_AND_ASSIGN(Step);
  };

  AccountMigrationRunner();
  ~AccountMigrationRunner();

  // Gets the current status of migration.
  Status GetStatus() const;

  // Adds a migration step. |AddStep| must be called before |Run| has been
  // called. Calls to |AddStep| create a dependency between the supplied
  // |step|s, i.e. |step|s will be executed in the order in which they were
  // supplied via |AddStep|. If any |step| fails, none of the following steps
  // will be executed.
  void AddStep(std::unique_ptr<Step> step);

  // Runs all the migration steps that have previously been added via |AddStep|.
  // |Run| must be called at most once during the lifetime of |this| object.
  // Subsequent calls to |Run| are silently ignored.
  // |callback| is called with the final result of the migration run.
  void Run(OnMigrationDone callback);

 private:
  // Runs the next migration step in |steps_|.
  void RunNextStep();

  // Callback from a migration |Step|.
  void OnStepCompleted(bool result);

  // Immediately finishes migration with a |Status::SUCCESS| and informs the
  // caller of |Run| about the result of the migration.
  void FinishWithSuccess();

  // Immediately finishes migration with a |Status::FAILURE| and informs the
  // caller of |Run| about the result of the migration.
  void FinishWithFailure();

  // Current status of migration.
  Status status_ = Status::kNotStarted;

  // A list of migration steps.
  std::queue<std::unique_ptr<Step>> steps_;

  // The current step being executed.
  std::unique_ptr<Step> current_step_ = nullptr;

  // Supplied by the caller of |Run| to get the overall result of migration.
  OnMigrationDone callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountMigrationRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccountMigrationRunner);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MIGRATION_RUNNER_H_
