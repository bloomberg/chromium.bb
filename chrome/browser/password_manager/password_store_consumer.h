// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
#pragma once

#include "content/browser/cancelable_request.h"

namespace webkit_glue {
struct PasswordForm;
};

// Reads from the PasswordStore are done asynchrously on a separate
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
      const std::vector<webkit_glue::PasswordForm*>& result) = 0;

  // The CancelableRequest framework needs both a callback (the
  // PasswordStoreConsumer::OnPasswordStoreRequestDone) as well as a
  // CancelableRequestConsumer.  This accessor makes the API simpler for callers
  // as PasswordStore can get both from the PasswordStoreConsumer.
  CancelableRequestConsumerBase* cancelable_consumer() {
    return &cancelable_consumer_;
  }

 protected:
  virtual ~PasswordStoreConsumer();

 private:
  CancelableRequestConsumer cancelable_consumer_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CONSUMER_H_
