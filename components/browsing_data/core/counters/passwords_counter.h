// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_

#include <memory>
#include <vector>

#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/sync/driver/sync_service_observer.h"

namespace browsing_data {

class PasswordsCounter : public browsing_data::BrowsingDataCounter,
                         public password_manager::PasswordStoreConsumer,
                         public password_manager::PasswordStore::Observer,
                         public syncer::SyncServiceObserver {
 public:
  class PasswordResult : public FinishedResult {
   public:
    PasswordResult(const PasswordsCounter* source,
                   ResultInt value,
                   bool password_sync_enabled);
    ~PasswordResult() override;

    bool password_sync_enabled() const { return password_sync_enabled_; }

   private:
    bool password_sync_enabled_;
  };

  explicit PasswordsCounter(
      scoped_refptr<password_manager::PasswordStore> store,
      syncer::SyncService* sync_service);
  ~PasswordsCounter() override;

  const char* GetPrefName() const override;

 private:
  void OnInitialized() override;

  // Counting is done asynchronously in a request to PasswordStore.
  // This callback returns the results, which are subsequently reported.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Called when the contents of the password store change. Triggers new
  // counting.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  void Count() override;

  base::CancelableTaskTracker cancelable_task_tracker_;
  scoped_refptr<password_manager::PasswordStore> store_;
  syncer::SyncService* sync_service_;
  bool password_sync_enabled_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_PASSWORDS_COUNTER_H_
