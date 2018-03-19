// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_
#define CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/chromeos_export.h"

namespace base {
class SequencedTaskRunner;
class ImportantFileWriter;
}  // namespace base

namespace chromeos {

class CHROMEOS_EXPORT AccountManager {
 public:
  // A map of account identifiers to login scoped tokens.
  using TokenMap = std::unordered_map<std::string, std::string>;

  // Note: |Initialize| MUST be called at least once on this object.
  AccountManager();
  ~AccountManager();

  // |home_dir| is the path of the Device Account's home directory (root of the
  // user's cryptohome). This method MUST be called at least once.
  void Initialize(const base::FilePath& home_dir);

  // Gets (async) a list of account identifiers known to |AccountManager|.
  void GetAccounts(base::OnceCallback<void(std::vector<std::string>)> callback);

  // Updates or inserts an LST (Login Scoped Token), for the account
  // corresponding to the given account id.
  void UpsertToken(const std::string& account_id,
                   const std::string& login_scoped_token);

 private:
  enum InitializationState {
    kNotStarted,   // Initialize has not been called
    kInProgress,   // Initialize has been called but not completed
    kInitialized,  // Initialization was successfully completed
  };

  friend class AccountManagerTest;
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestInitialization);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestUpsert);
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestPersistence);

  // Initializes |AccountManager| with the provided |task_runner| and location
  // of the user's home directory.
  void Initialize(const base::FilePath& home_dir,
                  scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Reads tokens from |tokens| and inserts them in |tokens_| and runs all
  // callbacks waiting on |AccountManager| initialization.
  void InsertTokensAndRunInitializationCallbacks(const TokenMap& tokens);

  // Accepts a closure and runs it immediately if |AccountManager| has already
  // been initialized, otherwise saves the |closure| for running later, when the
  // class is initialized.
  void RunOnInitialization(base::OnceClosure closure);

  // Does the actual work of getting a list of accounts. Assumes that
  // |AccountManager| initialization (|init_state_|) is complete.
  void GetAccountsInternal(
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Does the actual work of updating or inserting tokens. Assumes that
  // |AccountManager| initialization (|init_state_|) is complete.
  void UpsertTokenInternal(const std::string& account_id,
                           const std::string& login_scoped_token);

  // Persists (async) the current state of |tokens_| on disk.
  void PersistTokensAsync();

  // Status of this object's initialization.
  InitializationState init_state_ = InitializationState::kNotStarted;

  // A task runner for disk I/O.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::ImportantFileWriter> writer_;

  // A map of account ids to login scoped tokens.
  TokenMap tokens_;

  // Callbacks waiting on class initialization (|init_state_|).
  std::vector<base::OnceClosure> initialization_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_
