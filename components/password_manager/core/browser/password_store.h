// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_

#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "sync/api/syncable_service.h"

class Task;

namespace autofill {
struct PasswordForm;
}

namespace browser_sync {
class PasswordChangeProcessor;
class PasswordDataTypeController;
class PasswordModelAssociator;
class PasswordModelWorker;
}

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

namespace passwords_helper {
void AddLogin(password_manager::PasswordStore* store,
              const autofill::PasswordForm& form);
void RemoveLogin(password_manager::PasswordStore* store,
                 const autofill::PasswordForm& form);
void UpdateLogin(password_manager::PasswordStore* store,
                 const autofill::PasswordForm& form);
}

namespace syncer {
class SyncableService;
}

namespace password_manager {

class PasswordStoreConsumer;
class PasswordSyncableService;

// Interface for storing form passwords in a platform-specific secure way.
// The login request/manipulation API is not threadsafe and must be used
// from the UI thread.
// PasswordStoreSync is a hidden base class because only PasswordSyncableService
// needs to access these methods.
class PasswordStore : protected PasswordStoreSync,
                      public base::RefCountedThreadSafe<PasswordStore> {
 public:
  // Whether or not it's acceptable for Chrome to request access to locked
  // passwords, which requires prompting the user for permission.
  enum AuthorizationPromptPolicy {
    ALLOW_PROMPT,
    DISALLOW_PROMPT
  };

  // PasswordForm vector elements are meant to be owned by the
  // PasswordStoreConsumer. However, if the request is canceled after the
  // allocation, then the request must take care of the deletion.
  class GetLoginsRequest {
   public:
    explicit GetLoginsRequest(PasswordStoreConsumer* consumer);
    virtual ~GetLoginsRequest();

    void set_ignore_logins_cutoff(base::Time cutoff) {
      ignore_logins_cutoff_ = cutoff;
    }

    // Removes any logins in the result list that were saved before the cutoff.
    void ApplyIgnoreLoginsCutoff();

    // Forward the result to the consumer on the original message loop.
    void ForwardResult();

    std::vector<autofill::PasswordForm*>* result() const {
      return result_.get();
    }

   private:
    // See GetLogins(). Logins older than this will be removed from the reply.
    base::Time ignore_logins_cutoff_;

    base::WeakPtr<PasswordStoreConsumer> consumer_weak_;

    // The result of the request. It is filled in on the PasswordStore's task
    // thread and consumed on the UI thread.
    // TODO(dubroy): Remove this, and instead pass the vector directly to the
    // backend methods.
    scoped_ptr< std::vector<autofill::PasswordForm*> > result_;

    base::ThreadChecker thread_checker_;
    scoped_refptr<base::MessageLoopProxy> origin_loop_;

    DISALLOW_COPY_AND_ASSIGN(GetLoginsRequest);
  };

  // An interface used to notify clients (observers) of this object that data in
  // the password store has changed. Register the observer via
  // PasswordStore::AddObserver.
  class Observer {
   public:
    // Notifies the observer that password data changed. Will be called from
    // the UI thread.
    virtual void OnLoginsChanged(const PasswordStoreChangeList& changes) = 0;

   protected:
    virtual ~Observer() {}
  };

  PasswordStore(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner);

  // Reimplement this to add custom initialization. Always call this too.
  // |sync_username| is specified to aid in metrics reporting.
  virtual bool Init(const syncer::SyncableService::StartSyncFlare& flare,
                    const std::string& sync_username);

  // Adds the given PasswordForm to the secure password store asynchronously.
  virtual void AddLogin(const autofill::PasswordForm& form);

  // Updates the matching PasswordForm in the secure password store (async).
  virtual void UpdateLogin(const autofill::PasswordForm& form);

  // Removes the matching PasswordForm from the secure password store (async).
  virtual void RemoveLogin(const autofill::PasswordForm& form);

  // Removes all logins created in the given date range.
  virtual void RemoveLoginsCreatedBetween(base::Time delete_begin,
                                          base::Time delete_end);

  // Removes all logins synced in the given date range.
  virtual void RemoveLoginsSyncedBetween(base::Time delete_begin,
                                         base::Time delete_end);

  // Searches for a matching PasswordForm, and notifies |consumer| on
  // completion. The request will be cancelled if the consumer is destroyed.
  // |prompt_policy| indicates whether it's permissible to prompt the user to
  // authorize access to locked passwords. This argument is only used on
  // platforms that support prompting the user for access (such as Mac OS).
  // NOTE: This means that this method can return different results depending
  // on the value of |prompt_policy|.
  virtual void GetLogins(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy,
      PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are not blacklist entries--and
  // are thus auto-fillable. |consumer| will be notified on completion.
  // The request will be cancelled if the consumer is destroyed.
  virtual void GetAutofillableLogins(PasswordStoreConsumer* consumer);

  // Gets the complete list of PasswordForms that are blacklist entries,
  // and notify |consumer| on completion. The request will be cancelled if the
  // consumer is destroyed.
  virtual void GetBlacklistLogins(PasswordStoreConsumer* consumer);

  // Reports usage metrics for the database.
  virtual void ReportMetrics(const std::string& sync_username);

  // Adds an observer to be notified when the password store data changes.
  void AddObserver(Observer* observer);

  // Removes |observer| from the observer list.
  void RemoveObserver(Observer* observer);

  // Schedules the given |task| to be run on the PasswordStore's TaskRunner.
  bool ScheduleTask(const base::Closure& task);

  // Before you destruct the store, call Shutdown to indicate that the store
  // needs to shut itself down.
  virtual void Shutdown();

#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
  base::WeakPtr<syncer::SyncableService> GetPasswordSyncableService();
#endif

 protected:
  friend class base::RefCountedThreadSafe<PasswordStore>;
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest, IgnoreOldWwwGoogleLogins);

  typedef base::Callback<PasswordStoreChangeList(void)> ModificationTask;

  virtual ~PasswordStore();

  // Get the TaskRunner to use for PasswordStore background tasks.
  // By default, a SingleThreadTaskRunner on the DB thread is used, but
  // subclasses can override.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner();

  // Methods below will be run in PasswordStore's own thread.
  // Synchronous implementation that reports usage metrics.
  virtual void ReportMetricsImpl(const std::string& sync_username) = 0;

  // Bring PasswordStoreSync methods to the scope of PasswordStore. Otherwise,
  // base::Bind can't be used with them because it fails to cast PasswordStore
  // to PasswordStoreSync.
  virtual PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) = 0;
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) = 0;
  virtual PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) = 0;

  // Synchronous implementation to remove the given logins.
  virtual PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) = 0;

  typedef base::Callback<void(const std::vector<autofill::PasswordForm*>&)>
      ConsumerCallbackRunner;  // Owns all PasswordForms in the vector.

  // Should find all PasswordForms with the same signon_realm. The results
  // will then be scored by the PasswordFormManager. Once they are found
  // (or not), the consumer should be notified.
  virtual void GetLoginsImpl(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy,
      const ConsumerCallbackRunner& callback_runner) = 0;

  // Finds all non-blacklist PasswordForms, and notifies the consumer.
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request) = 0;

  // Finds all blacklist PasswordForms, and notifies the consumer.
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request) = 0;

  // Dispatches the result to the PasswordStoreConsumer on the original caller's
  // thread so the callback can be executed there. This should be the UI thread.
  virtual void ForwardLoginsResult(GetLoginsRequest* request);

  // Log UMA stats for number of bulk deletions.
  void LogStatsForBulkDeletion(int num_deletions);

  // TaskRunner for tasks that run on the main thread (usually the UI thread).
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  // TaskRunner for the DB thread. By default, this is the task runner used for
  // background tasks -- see |GetBackgroundTaskRunner|.
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner_;

  scoped_ptr<PasswordSyncableService> syncable_service_;

 private:
  // Schedule the given |func| to be run in the PasswordStore's own thread with
  // responses delivered to |consumer| on the current thread.
  template<typename BackendFunc>
  void Schedule(BackendFunc func, PasswordStoreConsumer* consumer);

  // Wrapper method called on the destination thread (DB for non-mac) that
  // invokes |task| and then calls back into the source thread to notify
  // observers that the password store may have been modified via
  // NotifyLoginsChanged(). Note that there is no guarantee that the called
  // method will actually modify the password store data.
  virtual void WrapModificationTask(ModificationTask task);

  // PasswordStoreSync:
  // Called by WrapModificationTask() once the underlying data-modifying
  // operation has been performed. Notifies observers that password store data
  // may have been changed.
  virtual void NotifyLoginsChanged(
      const PasswordStoreChangeList& changes) OVERRIDE;

  // Copies |matched_forms| into the request's result vector, then calls
  // |ForwardLoginsResult|. Temporarily used as an adapter between the API of
  // |GetLoginsImpl| and |PasswordStoreConsumer|.
  // TODO(dubroy): Get rid of this.
  void CopyAndForwardLoginsResult(
      PasswordStore::GetLoginsRequest* request,
      const std::vector<autofill::PasswordForm*>& matched_forms);

#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
  // Creates PasswordSyncableService instance on the background thread.
  void InitSyncableService(
      const syncer::SyncableService::StartSyncFlare& flare);

  // Deletes PasswordSyncableService instance on the background thread.
  void DestroySyncableService();
#endif

  // The observers.
  scoped_refptr<ObserverListThreadSafe<Observer> > observers_;

  bool shutdown_called_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStore);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_H_
