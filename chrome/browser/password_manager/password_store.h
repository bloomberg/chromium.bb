// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"

class PasswordStore;
class PasswordStoreConsumer;
class Task;

namespace browser_sync {
class PasswordDataTypeController;
class PasswordModelAssociator;
class PasswordModelWorker;
}

namespace webkit {
namespace forms {
struct PasswordForm;
}
}

namespace passwords_helper {
void AddLogin(PasswordStore* store, const webkit::forms::PasswordForm& form);
void RemoveLogin(PasswordStore* store, const webkit::forms::PasswordForm& form);
void UpdateLogin(PasswordStore* store, const webkit::forms::PasswordForm& form);
}

// Interface for storing form passwords in a platform-specific secure way.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
class PasswordStore
    : public RefcountedProfileKeyedService,
      public CancelableRequestProvider {
 public:
  typedef base::Callback<
      void(Handle, const std::vector<webkit::forms::PasswordForm*>&)>
      GetLoginsCallback;

  // PasswordForm vector elements are meant to be owned by the
  // PasswordStoreConsumer. However, if the request is canceled after the
  // allocation, then the request must take care of the deletion.
  // TODO(scr) If we can convert vector<PasswordForm*> to
  // ScopedVector<PasswordForm>, then we can move the following class to merely
  // a typedef. At the moment, a subclass of CancelableRequest1 is required to
  // provide a destructor, which cleans up after canceled requests by deleting
  // vector elements.
  class GetLoginsRequest
      : public CancelableRequest1<GetLoginsCallback,
                                  std::vector<webkit::forms::PasswordForm*> > {
   public:
    explicit GetLoginsRequest(const GetLoginsCallback& callback);

   protected:
    virtual ~GetLoginsRequest();

   private:
    DISALLOW_COPY_AND_ASSIGN(GetLoginsRequest);
  };

  // An interface used to notify clients (observers) of this object that data in
  // the password store has changed. Register the observer via
  // PasswordStore::SetObserver.
  class Observer {
   public:
    // Notifies the observer that password data changed in some way.
    virtual void OnLoginsChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  PasswordStore();

  // Reimplement this to add custom initialization. Always call this too.
  virtual bool Init();

  // Adds the given PasswordForm to the secure password store asynchronously.
  virtual void AddLogin(const webkit::forms::PasswordForm& form);

  // Updates the matching PasswordForm in the secure password store (async).
  void UpdateLogin(const webkit::forms::PasswordForm& form);

  // Removes the matching PasswordForm from the secure password store (async).
  void RemoveLogin(const webkit::forms::PasswordForm& form);

  // Removes all logins created in the given date range.
  void RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                  const base::Time& delete_end);

  // Searches for a matching PasswordForm and returns a handle so the async
  // request can be tracked. Implement the PasswordStoreConsumer interface to be
  // notified on completion.
  virtual Handle GetLogins(const webkit::forms::PasswordForm& form,
                           PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are not blacklist entries--and
  // are thus auto-fillable--and returns a handle so the async request can be
  // tracked. Implement the PasswordStoreConsumer interface to be notified on
  // completion.
  Handle GetAutofillableLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are blacklist entries, and
  // returns a handle so the async request can be tracked. Implement the
  // PasswordStoreConsumer interface to be notified on completion.
  Handle GetBlacklistLogins(PasswordStoreConsumer* consumer);

  // Reports usage metrics for the database.
  void ReportMetrics();

  // Adds an observer to be notified when the password store data changes.
  void AddObserver(Observer* observer);

  // Removes |observer| from the observer list.
  void RemoveObserver(Observer* observer);

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;
  friend class browser_sync::PasswordDataTypeController;
  friend class browser_sync::PasswordModelAssociator;
  friend class browser_sync::PasswordModelWorker;
  friend void passwords_helper::AddLogin(PasswordStore*,
                                         const webkit::forms::PasswordForm&);
  friend void passwords_helper::RemoveLogin(PasswordStore*,
                                            const webkit::forms::PasswordForm&);
  friend void passwords_helper::UpdateLogin(PasswordStore*,
                                            const webkit::forms::PasswordForm&);

  virtual ~PasswordStore();

  // Provided to allow subclasses to extend GetLoginsRequest if additional info
  // is needed between a call and its Impl.
  virtual GetLoginsRequest* NewGetLoginsRequest(
      const GetLoginsCallback& callback);

  // Schedule the given |task| to be run in the PasswordStore's own thread.
  virtual bool ScheduleTask(const base::Closure& task);

  // These will be run in PasswordStore's own thread.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl() = 0;
  // Synchronous implementation to add the given login.
  virtual void AddLoginImpl(const webkit::forms::PasswordForm& form) = 0;
  // Synchronous implementation to update the given login.
  virtual void UpdateLoginImpl(const webkit::forms::PasswordForm& form) = 0;
  // Synchronous implementation to remove the given login.
  virtual void RemoveLoginImpl(const webkit::forms::PasswordForm& form) = 0;
  // Synchronous implementation to remove the given logins.
  virtual void RemoveLoginsCreatedBetweenImpl(const base::Time& delete_begin,
                                              const base::Time& delete_end) = 0;
  // Should find all PasswordForms with the same signon_realm. The results
  // will then be scored by the PasswordFormManager. Once they are found
  // (or not), the consumer should be notified.
  virtual void GetLoginsImpl(GetLoginsRequest* request,
                             const webkit::forms::PasswordForm& form) = 0;
  // Finds all non-blacklist PasswordForms, and notifies the consumer.
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request) = 0;
  // Finds all blacklist PasswordForms, and notifies the consumer.
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request) = 0;

  // Finds all non-blacklist PasswordForms, and fills the vector.
  virtual bool FillAutofillableLogins(
      std::vector<webkit::forms::PasswordForm*>* forms) = 0;
  // Finds all blacklist PasswordForms, and fills the vector.
  virtual bool FillBlacklistLogins(
      std::vector<webkit::forms::PasswordForm*>* forms) = 0;

  // Dispatches the result to the PasswordStoreConsumer on the original caller's
  // thread so the callback can be executed there.  This should be the UI
  // thread.
  virtual void ForwardLoginsResult(GetLoginsRequest* request);

  // Schedule the given |func| to be run in the PasswordStore's own thread with
  // responses delivered to |consumer| on the current thread.
  template<typename BackendFunc>
  Handle Schedule(BackendFunc func, PasswordStoreConsumer* consumer);

  // Schedule the given |func| to be run in the PasswordStore's own thread with
  // argument |a| and responses delivered to |consumer| on the current thread.
  template<typename BackendFunc, typename ArgA>
  Handle Schedule(BackendFunc func, PasswordStoreConsumer* consumer,
                  const ArgA& a);

 private:
  // Wrapper method called on the destination thread (DB for non-mac) that
  // invokes |task| and then calls back into the source thread to notify
  // observers that the password store may have been modified via
  // NotifyLoginsChanged(). Note that there is no guarantee that the called
  // method will actually modify the password store data.
  void WrapModificationTask(base::Closure task);

  // Post a message to the UI thread to run NotifyLoginsChanged(). Called by
  // WrapModificationTask() above, and split out as a separate method so that
  // password sync can call it as well after synchronously updating the password
  // store.
  void PostNotifyLoginsChanged();

  // Called by WrapModificationTask() once the underlying data-modifying
  // operation has been performed. Notifies observers that password store data
  // may have been changed.
  void NotifyLoginsChanged();

  // The observers.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_H_
