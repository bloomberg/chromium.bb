// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_

#include "chrome/common/cancelable_task_tracker.h"

namespace autofill {
struct PasswordForm;
}

// Reads from the PasswordStore are done asynchronously on a separate
// thread. PasswordStoreConsumer provides the virtual callback method, which is
// guaranteed to be executed on this (the UI) thread. It also provides the
// CancelableTaskTracker member, which cancels any outstanding tasks upon
// destruction.
class PasswordStoreConsumer {
 public:
  PasswordStoreConsumer();

  // Called when the request is finished. If there are no results, it is called
  // with an empty vector.
  // Note: The implementation owns all PasswordForms in the vector.
  virtual void OnGetPasswordStoreResults(
      const std::vector<autofill::PasswordForm*>& results) = 0;

  // The CancelableTaskTracker can be used for cancelling the tasks associated
  // with the consumer.
  CancelableTaskTracker* cancelable_task_tracker() {
    return &cancelable_task_tracker_;
  }

  base::WeakPtr<PasswordStoreConsumer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  virtual ~PasswordStoreConsumer();

 private:
  CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<PasswordStoreConsumer> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
