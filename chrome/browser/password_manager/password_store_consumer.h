// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_

#include "chrome/browser/common/cancelable_request.h"
#include "chrome/common/cancelable_task_tracker.h"

namespace content {
struct PasswordForm;
}

// Reads from the PasswordStore are done asynchronously on a separate
// thread. PasswordStoreConsumer provides the virtual callback method, which is
// guaranteed to be executed on this (the UI) thread. It also provides the
// CancelableRequestConsumer member, which cancels any outstanding requests upon
// destruction.
class PasswordStoreConsumer {
 public:
  PasswordStoreConsumer();

  // Call this when the request is finished. If there are no results, call it
  // anyway with an empty vector.
  virtual void OnPasswordStoreRequestDone(
      CancelableRequestProvider::Handle handle,
      const std::vector<content::PasswordForm*>& result) = 0;

  // The CancelableRequest framework needs both a callback (the
  // PasswordStoreConsumer::OnPasswordStoreRequestDone) as well as a
  // CancelableRequestConsumer.  This accessor makes the API simpler for callers
  // as PasswordStore can get both from the PasswordStoreConsumer.
  CancelableRequestConsumerBase* cancelable_consumer() {
    return &cancelable_consumer_;
  }

  // Called when the request is finished. If there are no results, it is called
  // with an empty vector.
  // Note: The implementation owns all PasswordForms in the vector.
  virtual void OnGetPasswordStoreResults(
      const std::vector<content::PasswordForm*>& results) = 0;

  // Cancels the task.
  CancelableTaskTracker* cancelable_task_tracker() {
    return &cancelable_task_tracker_;
  }

 protected:
  virtual ~PasswordStoreConsumer();

 private:
  CancelableRequestConsumer cancelable_consumer_;
  CancelableTaskTracker cancelable_task_tracker_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
