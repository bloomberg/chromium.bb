// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_PASSWORDS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_PASSWORDS_COUNTER_H_

#include "components/browsing_data/counters/browsing_data_counter.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class Profile;

class PasswordsCounter : public browsing_data::BrowsingDataCounter,
                         public password_manager::PasswordStoreConsumer,
                         public password_manager::PasswordStore::Observer {
 public:
  explicit PasswordsCounter(Profile* profile);
  ~PasswordsCounter() override;

 private:
  base::CancelableTaskTracker cancelable_task_tracker_;
  scoped_refptr<password_manager::PasswordStore> store_;

  Profile* profile_;

  void OnInitialized() override;

  // Counting is done asynchronously in a request to PasswordStore.
  // This callback returns the results, which are subsequently reported.
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

  // Called when the contents of the password store change. Triggers new
  // counting.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void Count() override;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_PASSWORDS_COUNTER_H_
