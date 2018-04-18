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
#include "base/observer_list.h"
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

  // A callback for list of Account Id Keys.
  using AccountListCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // Called when the list of accounts known to |AccountManager| is updated.
    // Use |AccountManager::AddObserver| to add an |Observer|.
    // Note: This is not called when the refresh token for an already known
    // account is updated.
    // Note: |Observer|s which register with |AccountManager| before its
    // initialization is complete will get notified when |AccountManager| is
    // fully initialized.
    // Note: |Observer|s which register with |AccountManager| after its
    // initialization is complete will not get an immediate
    // notification-on-registration.
    virtual void OnAccountListUpdated(
        const std::vector<std::string>& accounts) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Note: |Initialize| MUST be called at least once on this object.
  AccountManager();
  ~AccountManager();

  // |home_dir| is the path of the Device Account's home directory (root of the
  // user's cryptohome). This method MUST be called at least once.
  void Initialize(const base::FilePath& home_dir);

  // Gets (async) a list of account identifiers known to |AccountManager|.
  void GetAccounts(AccountListCallback callback);

  // Updates or inserts an LST (Login Scoped Token), for the account
  // corresponding to the given account id.
  void UpsertToken(const std::string& account_id,
                   const std::string& login_scoped_token);

  // Add a non owning pointer to an |AccountManager::Observer|.
  void AddObserver(Observer* observer);

  // Removes an |AccountManager::Observer|. Does nothing if the |observer| is
  // not in the list of known observers.
  void RemoveObserver(Observer* observer);

 private:
  enum InitializationState {
    kNotStarted,   // Initialize has not been called
    kInProgress,   // Initialize has been called but not completed
    kInitialized,  // Initialization was successfully completed
  };

  friend class AccountManagerTest;
  FRIEND_TEST_ALL_PREFIXES(AccountManagerTest, TestInitialization);
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
  void GetAccountsInternal(AccountListCallback callback);

  // Does the actual work of updating or inserting tokens. Assumes that
  // |AccountManager| initialization (|init_state_|) is complete.
  void UpsertTokenInternal(const std::string& account_id,
                           const std::string& login_scoped_token);

  // Posts a task on |task_runner_|, which is usually a background thread, to
  // persist the current state of |tokens_|.
  void PersistTokensAsync();

  // Notify |Observer|s about an account list update.
  void NotifyAccountListObservers();

  // Status of this object's initialization.
  InitializationState init_state_ = InitializationState::kNotStarted;

  // A task runner for disk I/O.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::ImportantFileWriter> writer_;

  // A map of account ids to login scoped tokens.
  TokenMap tokens_;

  // Callbacks waiting on class initialization (|init_state_|).
  std::vector<base::OnceClosure> initialization_callbacks_;

  // A list of |AccountManager| observers.
  // Verifies that the list is empty on destruction.
  base::ObserverList<Observer, true /* check_empty */> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccountManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_H_
