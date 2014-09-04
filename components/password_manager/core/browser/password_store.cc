// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_syncable_service.h"

#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
#include "components/password_manager/core/browser/password_syncable_service.h"
#endif

using autofill::PasswordForm;

namespace password_manager {

namespace {

// Calls |consumer| back with the request result, if |consumer| is still alive.
// Takes ownership of the elements in |result|, passing ownership to |consumer|
// if it is still alive.
void MaybeCallConsumerCallback(base::WeakPtr<PasswordStoreConsumer> consumer,
                               scoped_ptr<std::vector<PasswordForm*> > result) {
  if (consumer.get())
    consumer->OnGetPasswordStoreResults(*result);
  else
    STLDeleteElements(result.get());
}

// http://crbug.com/404012. Let's see where the empty fields come from.
void CheckForEmptyUsernameAndPassword(const PasswordForm& form) {
  if (form.username_value.empty() &&
      form.password_value.empty() &&
      !form.blacklisted_by_user)
    base::debug::DumpWithoutCrashing();
}

}  // namespace

PasswordStore::GetLoginsRequest::GetLoginsRequest(
    PasswordStoreConsumer* consumer)
    : consumer_weak_(consumer->GetWeakPtr()),
      result_(new std::vector<PasswordForm*>()) {
  DCHECK(thread_checker_.CalledOnValidThread());
  origin_loop_ = base::MessageLoopProxy::current();
}

PasswordStore::GetLoginsRequest::~GetLoginsRequest() {
}

void PasswordStore::GetLoginsRequest::ApplyIgnoreLoginsCutoff() {
  if (!ignore_logins_cutoff_.is_null()) {
    // Count down rather than up since we may be deleting elements.
    // Note that in principle it could be more efficient to copy the whole array
    // since that's worst-case linear time, but we expect that elements will be
    // deleted rarely and lists will be small, so this avoids the copies.
    for (size_t i = result_->size(); i > 0; --i) {
      if ((*result_)[i - 1]->date_created < ignore_logins_cutoff_) {
        delete (*result_)[i - 1];
        result_->erase(result_->begin() + (i - 1));
      }
    }
  }
}

void PasswordStore::GetLoginsRequest::ForwardResult() {
  origin_loop_->PostTask(FROM_HERE,
                         base::Bind(&MaybeCallConsumerCallback,
                                    consumer_weak_,
                                    base::Passed(result_.Pass())));
}

PasswordStore::PasswordStore(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner)
    : main_thread_runner_(main_thread_runner),
      db_thread_runner_(db_thread_runner),
      observers_(new ObserverListThreadSafe<Observer>()),
      shutdown_called_(false) {}

bool PasswordStore::Init(const syncer::SyncableService::StartSyncFlare& flare,
                         const std::string& sync_username) {
  ReportMetrics(sync_username);
#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
  ScheduleTask(base::Bind(&PasswordStore::InitSyncableService, this, flare));
#endif
  return true;
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  CheckForEmptyUsernameAndPassword(form);
  ScheduleTask(
      base::Bind(&PasswordStore::WrapModificationTask, this,
                 base::Bind(&PasswordStore::AddLoginImpl, this, form)));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  CheckForEmptyUsernameAndPassword(form);
  ScheduleTask(
      base::Bind(&PasswordStore::WrapModificationTask, this,
                 base::Bind(&PasswordStore::UpdateLoginImpl, this, form)));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  ScheduleTask(
      base::Bind(&PasswordStore::WrapModificationTask, this,
                 base::Bind(&PasswordStore::RemoveLoginImpl, this, form)));
}

void PasswordStore::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                               base::Time delete_end) {
  ScheduleTask(
      base::Bind(&PasswordStore::WrapModificationTask, this,
                 base::Bind(&PasswordStore::RemoveLoginsCreatedBetweenImpl,
                            this, delete_begin, delete_end)));
}

void PasswordStore::RemoveLoginsSyncedBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  ScheduleTask(
      base::Bind(&PasswordStore::WrapModificationTask,
                 this,
                 base::Bind(&PasswordStore::RemoveLoginsSyncedBetweenImpl,
                            this,
                            delete_begin,
                            delete_end)));
}

void PasswordStore::GetLogins(
    const PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy,
    PasswordStoreConsumer* consumer) {
  // Per http://crbug.com/121738, we deliberately ignore saved logins for
  // http*://www.google.com/ that were stored prior to 2012. (Google now uses
  // https://accounts.google.com/ for all login forms, so these should be
  // unused.) We don't delete them just yet, and they'll still be visible in the
  // password manager, but we won't use them to autofill any forms. This is a
  // security feature to help minimize damage that can be done by XSS attacks.
  // TODO(mdm): actually delete them at some point, say M24 or so.
  base::Time ignore_logins_cutoff;  // the null time
  if (form.scheme == PasswordForm::SCHEME_HTML &&
      (form.signon_realm == "http://www.google.com" ||
       form.signon_realm == "http://www.google.com/" ||
       form.signon_realm == "https://www.google.com" ||
       form.signon_realm == "https://www.google.com/")) {
    static const base::Time::Exploded exploded_cutoff =
        { 2012, 1, 0, 1, 0, 0, 0, 0 };  // 00:00 Jan 1 2012
    ignore_logins_cutoff = base::Time::FromUTCExploded(exploded_cutoff);
  }
  GetLoginsRequest* request = new GetLoginsRequest(consumer);
  request->set_ignore_logins_cutoff(ignore_logins_cutoff);

  ConsumerCallbackRunner callback_runner =
      base::Bind(&PasswordStore::CopyAndForwardLoginsResult,
                 this, base::Owned(request));
  ScheduleTask(base::Bind(&PasswordStore::GetLoginsImpl,
                          this, form, prompt_policy, callback_runner));
}

void PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetAutofillableLoginsImpl, consumer);
}

void PasswordStore::GetBlacklistLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetBlacklistLoginsImpl, consumer);
}

void PasswordStore::ReportMetrics(const std::string& sync_username) {
  ScheduleTask(base::Bind(&PasswordStore::ReportMetricsImpl, this,
                          sync_username));
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

bool PasswordStore::ScheduleTask(const base::Closure& task) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      GetBackgroundTaskRunner());
  if (task_runner.get())
    return task_runner->PostTask(FROM_HERE, task);
  return false;
}

void PasswordStore::Shutdown() {
#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
  ScheduleTask(base::Bind(&PasswordStore::DestroySyncableService, this));
#endif
  shutdown_called_ = true;
}

#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
base::WeakPtr<syncer::SyncableService>
    PasswordStore::GetPasswordSyncableService() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(syncable_service_);
  return syncable_service_->AsWeakPtr();
}
#endif

PasswordStore::~PasswordStore() { DCHECK(shutdown_called_); }

scoped_refptr<base::SingleThreadTaskRunner>
PasswordStore::GetBackgroundTaskRunner() {
  return db_thread_runner_;
}

void PasswordStore::ForwardLoginsResult(GetLoginsRequest* request) {
  request->ApplyIgnoreLoginsCutoff();
  request->ForwardResult();
}

void PasswordStore::CopyAndForwardLoginsResult(
    PasswordStore::GetLoginsRequest* request,
    const std::vector<PasswordForm*>& matched_forms) {
  // Copy the contents of |matched_forms| into the request. The request takes
  // ownership of the PasswordForm elements.
  *(request->result()) = matched_forms;
  ForwardLoginsResult(request);
}

void PasswordStore::LogStatsForBulkDeletion(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedByBulkDelete",
                       num_deletions);
}

void PasswordStore::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!changes.empty()) {
    observers_->Notify(&Observer::OnLoginsChanged, changes);
#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
    if (syncable_service_)
      syncable_service_->ActOnPasswordStoreChanges(changes);
#endif
  }
}

template<typename BackendFunc>
void PasswordStore::Schedule(
    BackendFunc func,
    PasswordStoreConsumer* consumer) {
  GetLoginsRequest* request = new GetLoginsRequest(consumer);
  consumer->cancelable_task_tracker()->PostTask(
      GetBackgroundTaskRunner().get(),
      FROM_HERE,
      base::Bind(func, this, base::Owned(request)));
}

void PasswordStore::WrapModificationTask(ModificationTask task) {
  PasswordStoreChangeList changes = task.Run();
  NotifyLoginsChanged(changes);
}

#if defined(PASSWORD_MANAGER_ENABLE_SYNC)
void PasswordStore::InitSyncableService(
    const syncer::SyncableService::StartSyncFlare& flare) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  DCHECK(!syncable_service_);
  syncable_service_.reset(new PasswordSyncableService(this));
  syncable_service_->InjectStartSyncFlare(flare);
}

void PasswordStore::DestroySyncableService() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  syncable_service_.reset();
}
#endif

}  // namespace password_manager
