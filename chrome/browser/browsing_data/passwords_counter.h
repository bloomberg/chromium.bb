// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_PASSWORDS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_PASSWORDS_COUNTER_H_

#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class PasswordsCounter: public BrowsingDataCounter,
                        public password_manager::PasswordStoreConsumer,
                        public password_manager::PasswordStore::Observer {
 public:
  PasswordsCounter();
  ~PasswordsCounter() override;

  const std::string& GetPrefName() const override;

 private:
  const std::string pref_name_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  password_manager::PasswordStore* store_ = nullptr;

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
