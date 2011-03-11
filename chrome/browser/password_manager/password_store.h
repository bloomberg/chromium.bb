// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_H_
#pragma once

#include <set>
#include <vector>

#include "base/ref_counted.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "webkit/glue/password_form.h"

class Profile;
class Task;

namespace browser_sync {
class PasswordDataTypeController;
class PasswordModelAssociator;
class PasswordModelWorker;
};

class PasswordStoreConsumer {
 public:
  virtual ~PasswordStoreConsumer() {}
  // Call this when the request is finished. If there are no results, call it
  // anyway with an empty vector.
  virtual void OnPasswordStoreRequestDone(
      int handle, const std::vector<webkit_glue::PasswordForm*>& result) = 0;
};

// Interface for storing form passwords in a platform-specific secure way.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
class PasswordStore : public base::RefCountedThreadSafe<PasswordStore> {
 public:
  PasswordStore();

  // Reimplement this to add custom initialization. Always call this too.
  virtual bool Init();

  // Adds the given PasswordForm to the secure password store asynchronously.
  virtual void AddLogin(const webkit_glue::PasswordForm& form);

  // Updates the matching PasswordForm in the secure password store (async).
  void UpdateLogin(const webkit_glue::PasswordForm& form);

  // Removes the matching PasswordForm from the secure password store (async).
  void RemoveLogin(const webkit_glue::PasswordForm& form);

  // Removes all logins created in the given date range.
  void RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                  const base::Time& delete_end);

  // Searches for a matching PasswordForm and returns a handle so the async
  // request can be tracked. Implement the PasswordStoreConsumer interface to
  // be notified on completion.
  virtual int GetLogins(const webkit_glue::PasswordForm& form,
                        PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are not blacklist entries--and
  // are thus auto-fillable--and returns a handle so the async request can be
  // tracked. Implement the PasswordStoreConsumer interface to be notified
  // on completion.
  int GetAutofillableLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are blacklist entries, and
  // returns a handle so the async request can be tracked. Implement the
  // PasswordStoreConsumer interface to be notified on completion.
  int GetBlacklistLogins(PasswordStoreConsumer* consumer);

  // Cancels a previous Get*Logins query (async)
  void CancelLoginsQuery(int handle);

  // Reports usage metrics for the database.
  virtual void ReportMetrics();

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;
  friend class browser_sync::PasswordDataTypeController;
  friend class browser_sync::PasswordModelAssociator;
  friend class browser_sync::PasswordModelWorker;
  friend class LivePasswordsSyncTest;

  virtual ~PasswordStore();

  // Simple container class that represents a login lookup request.
  class GetLoginsRequest {
   public:
    GetLoginsRequest(PasswordStoreConsumer* c,
                     int handle);

    // The consumer to notify when this request is complete.
    PasswordStoreConsumer* consumer;
    // A unique handle for the request
    int handle;
    // The message loop that the request was made from.  We send the result
    // back to the consumer in this same message loop.
    MessageLoop* message_loop;

   private:
    DISALLOW_COPY_AND_ASSIGN(GetLoginsRequest);
  };

  // Schedule the given task to be run in the PasswordStore's own thread.
  virtual void ScheduleTask(Task* task);

  // These will be run in PasswordStore's own thread.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl() = 0;
  // Synchronous implementation to add the given login.
  virtual void AddLoginImpl(const webkit_glue::PasswordForm& form) = 0;
  // Synchronous implementation to update the given login.
  virtual void UpdateLoginImpl(const webkit_glue::PasswordForm& form) = 0;
  // Synchronous implementation to remove the given login.
  virtual void RemoveLoginImpl(const webkit_glue::PasswordForm& form) = 0;
  // Synchronous implementation to remove the given logins.
  virtual void RemoveLoginsCreatedBetweenImpl(const base::Time& delete_begin,
                                              const base::Time& delete_end) = 0;
  // Should find all PasswordForms with the same signon_realm. The results
  // will then be scored by the PasswordFormManager. Once they are found
  // (or not), the consumer should be notified.
  virtual void GetLoginsImpl(GetLoginsRequest* request,
                             const webkit_glue::PasswordForm& form) = 0;
  // Finds all non-blacklist PasswordForms, and notifies the consumer.
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request) = 0;
  // Finds all blacklist PasswordForms, and notifies the consumer.
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request) = 0;

  // Finds all non-blacklist PasswordForms, and fills the vector.
  virtual bool FillAutofillableLogins(
      std::vector<webkit_glue::PasswordForm*>* forms) = 0;
  // Finds all blacklist PasswordForms, and fills the vector.
  virtual bool FillBlacklistLogins(
      std::vector<webkit_glue::PasswordForm*>* forms) = 0;

  // Notifies the consumer that a Get*Logins() request is complete.
  virtual void NotifyConsumer(
      GetLoginsRequest* request,
      const std::vector<webkit_glue::PasswordForm*>& forms);

 private:
  // Called by NotifyConsumer, but runs in the consumer's thread.  Will not
  // call the consumer if the request was canceled.  This extra layer is here so
  // that PasswordStoreConsumer doesn't have to be reference counted (we assume
  // consumers will cancel their requests before they are destroyed).
  void NotifyConsumerImpl(PasswordStoreConsumer* consumer, int handle,
                          const std::vector<webkit_glue::PasswordForm*>& forms);

  // Returns a new request handle tracked in pending_requests_.
  int GetNewRequestHandle();

  // Next handle to return from Get*Logins() to allow callers to track
  // their request.
  int handle_;

  // List of pending request handles.  Handles are removed from the set when
  // they finish or are canceled.
  std::set<int> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_H_
