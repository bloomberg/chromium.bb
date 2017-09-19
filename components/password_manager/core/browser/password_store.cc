// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_syncable_service.h"
#include "components/password_manager/core/browser/statistics_table.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#endif

using autofill::PasswordForm;

namespace password_manager {

PasswordStore::GetLoginsRequest::GetLoginsRequest(
    PasswordStoreConsumer* consumer)
    : consumer_weak_(consumer->GetWeakPtr()) {
  origin_task_runner_ = base::SequencedTaskRunnerHandle::Get();
}

PasswordStore::GetLoginsRequest::~GetLoginsRequest() {
}

void PasswordStore::GetLoginsRequest::NotifyConsumerWithResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  if (!ignore_logins_cutoff_.is_null()) {
    base::EraseIf(results,
                  [this](const std::unique_ptr<PasswordForm>& credential) {
                    return (credential->date_created < ignore_logins_cutoff_);
                  });
  }

  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PasswordStoreConsumer::OnGetPasswordStoreResults,
                            consumer_weak_, base::Passed(&results)));
}

void PasswordStore::GetLoginsRequest::NotifyWithSiteStatistics(
    std::vector<InteractionsStats> stats) {
  origin_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PasswordStoreConsumer::OnGetSiteStatistics,
                            consumer_weak_, base::Passed(&stats)));
}

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
PasswordStore::CheckReuseRequest::CheckReuseRequest(
    PasswordReuseDetectorConsumer* consumer)
    : origin_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      consumer_weak_(consumer->AsWeakPtr()) {}

PasswordStore::CheckReuseRequest::~CheckReuseRequest() {}

void PasswordStore::CheckReuseRequest::OnReuseFound(
    size_t password_length,
    bool matches_sync_password,
    const std::vector<std::string>& matching_domains,
    int saved_passwords) {
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PasswordReuseDetectorConsumer::OnReuseFound, consumer_weak_,
                 password_length, matches_sync_password, matching_domains,
                 saved_passwords));
}
#endif

PasswordStore::FormDigest::FormDigest(autofill::PasswordForm::Scheme new_scheme,
                                      const std::string& new_signon_realm,
                                      const GURL& new_origin)
    : scheme(new_scheme), signon_realm(new_signon_realm), origin(new_origin) {}

PasswordStore::FormDigest::FormDigest(const PasswordForm& form)
    : scheme(form.scheme),
      signon_realm(form.signon_realm),
      origin(form.origin) {}

PasswordStore::FormDigest::FormDigest(const FormDigest& other) = default;

PasswordStore::FormDigest::FormDigest(FormDigest&& other) = default;

PasswordStore::FormDigest& PasswordStore::FormDigest::operator=(
    const FormDigest& other) = default;

PasswordStore::FormDigest& PasswordStore::FormDigest::operator=(
    FormDigest&& other) = default;

bool PasswordStore::FormDigest::operator==(const FormDigest& other) const {
  return scheme == other.scheme && signon_realm == other.signon_realm &&
         origin == other.origin;
}

PasswordStore::PasswordStore()
    : observers_(new base::ObserverListThreadSafe<Observer>()),
      is_propagating_password_changes_to_web_credentials_enabled_(false),
      shutdown_called_(false) {}

bool PasswordStore::Init(const syncer::SyncableService::StartSyncFlare& flare,
                         PrefService* prefs) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);
  background_task_runner_ = CreateBackgroundTaskRunner();
  DCHECK(background_task_runner_);
  ScheduleTask(
      base::Bind(&PasswordStore::InitOnBackgroundSequence, this, flare));
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  hash_password_manager_.set_prefs(prefs);
  ScheduleTask(
      base::Bind(&PasswordStore::SaveSyncPasswordHashImpl, this,
                 base::Passed(hash_password_manager_.RetrievePasswordHash())));
#endif
  return true;
}

void PasswordStore::SetAffiliatedMatchHelper(
    std::unique_ptr<AffiliatedMatchHelper> helper) {
  affiliated_match_helper_ = std::move(helper);
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::AddLoginInternal, this, form));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::UpdateLoginInternal, this, form));
}

void PasswordStore::UpdateLoginWithPrimaryKey(
    const autofill::PasswordForm& new_form,
    const autofill::PasswordForm& old_primary_key) {
  ScheduleTask(base::Bind(&PasswordStore::UpdateLoginWithPrimaryKeyInternal,
                          this, new_form, old_primary_key));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginInternal, this, form));
}

void PasswordStore::RemoveLoginsByURLAndTime(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsByURLAndTimeInternal,
                          this, url_filter, delete_begin, delete_end,
                          completion));
}

void PasswordStore::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsCreatedBetweenInternal,
                          this, delete_begin, delete_end, completion));
}

void PasswordStore::RemoveLoginsSyncedBetween(base::Time delete_begin,
                                              base::Time delete_end) {
  ScheduleTask(base::Bind(&PasswordStore::RemoveLoginsSyncedBetweenInternal,
                          this, delete_begin, delete_end));
}

void PasswordStore::RemoveStatisticsByOriginAndTime(
    const base::Callback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  ScheduleTask(
      base::Bind(&PasswordStore::RemoveStatisticsByOriginAndTimeInternal, this,
                 origin_filter, delete_begin, delete_end, completion));
}

void PasswordStore::DisableAutoSignInForOrigins(
    const base::Callback<bool(const GURL&)>& origin_filter,
    const base::Closure& completion) {
  ScheduleTask(
      base::Bind(&PasswordStore::DisableAutoSignInForOriginsInternal, this,
                 base::Callback<bool(const GURL&)>(origin_filter), completion));
}

void PasswordStore::GetLogins(const FormDigest& form,
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
    base::Time out_time;
    bool conversion_success =
        base::Time::FromUTCExploded(exploded_cutoff, &out_time);
    DCHECK(conversion_success);
    ignore_logins_cutoff = out_time;
  }
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  request->set_ignore_logins_cutoff(ignore_logins_cutoff);

  if (affiliated_match_helper_) {
    affiliated_match_helper_->GetAffiliatedAndroidRealms(
        form, base::Bind(&PasswordStore::ScheduleGetLoginsWithAffiliations,
                         this, form, base::Passed(&request)));
  } else {
    ScheduleTask(base::Bind(&PasswordStore::GetLoginsImpl, this, form,
                            base::Passed(&request)));
  }
}

void PasswordStore::GetLoginsForSameOrganizationName(
    const std::string& signon_realm,
    PasswordStoreConsumer* consumer) {
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  ScheduleTask(base::Bind(&PasswordStore::GetLoginsForSameOrganizationNameImpl,
                          this, signon_realm, base::Passed(&request)));
}

void PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetAutofillableLoginsImpl, consumer);
}

void PasswordStore::GetAutofillableLoginsWithAffiliationAndBrandingInformation(
    PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::
               GetAutofillableLoginsWithAffiliationAndBrandingInformationImpl,
           consumer);
}

void PasswordStore::GetBlacklistLogins(PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::GetBlacklistLoginsImpl, consumer);
}

void PasswordStore::GetBlacklistLoginsWithAffiliationAndBrandingInformation(
    PasswordStoreConsumer* consumer) {
  Schedule(&PasswordStore::
               GetBlacklistLoginsWithAffiliationAndBrandingInformationImpl,
           consumer);
}

void PasswordStore::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled) {
  if (background_task_runner_) {
    base::Closure task =
        base::Bind(&PasswordStore::ReportMetricsImpl, this, sync_username,
                   custom_passphrase_sync_enabled);
    background_task_runner_->PostDelayedTask(FROM_HERE, task,
                                             base::TimeDelta::FromSeconds(30));
  }

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS)) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (!sync_username.empty())
    hash_password_manager_.ReportIsSyncPasswordHashSavedMetric();
#endif
}

void PasswordStore::AddSiteStats(const InteractionsStats& stats) {
  ScheduleTask(base::Bind(&PasswordStore::AddSiteStatsImpl, this, stats));
}

void PasswordStore::RemoveSiteStats(const GURL& origin_domain) {
  ScheduleTask(
      base::Bind(&PasswordStore::RemoveSiteStatsImpl, this, origin_domain));
}

void PasswordStore::GetAllSiteStats(PasswordStoreConsumer* consumer) {
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  ScheduleTask(base::Bind(&PasswordStore::NotifyAllSiteStats, this,
                          base::Passed(&request)));
}

void PasswordStore::GetSiteStats(const GURL& origin_domain,
                                 PasswordStoreConsumer* consumer) {
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  ScheduleTask(base::Bind(&PasswordStore::NotifySiteStats, this, origin_domain,
                          base::Passed(&request)));
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

bool PasswordStore::ScheduleTask(const base::Closure& task) {
  if (background_task_runner_)
    return background_task_runner_->PostTask(FROM_HERE, task);
  return false;
}

void PasswordStore::ShutdownOnUIThread() {
  ScheduleTask(base::Bind(&PasswordStore::DestroyOnBackgroundSequence, this));
  // The AffiliationService must be destroyed from the main sequence.
  affiliated_match_helper_.reset();
  shutdown_called_ = true;
#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  if (notifier_)
    notifier_->UnsubscribeFromSigninEvents();
#endif
}

base::WeakPtr<syncer::SyncableService>
PasswordStore::GetPasswordSyncableService() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(syncable_service_);
  return syncable_service_->AsWeakPtr();
}

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
void PasswordStore::CheckReuse(const base::string16& input,
                               const std::string& domain,
                               PasswordReuseDetectorConsumer* consumer) {
  auto check_reuse_request = std::make_unique<CheckReuseRequest>(consumer);
  ScheduleTask(base::Bind(&PasswordStore::CheckReuseImpl, this,
                          base::Passed(&check_reuse_request), input, domain));
}
#endif

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
void PasswordStore::SaveSyncPasswordHash(const base::string16& password) {
  // TODO(crbug.com/657041): Log success of saving password hash to UMA.
  hash_password_manager_.SavePasswordHash(password);
  base::Optional<SyncPasswordData> sync_password_data =
      hash_password_manager_.RetrievePasswordHash();
  ScheduleTask(base::Bind(&PasswordStore::SaveSyncPasswordHashImpl, this,
                          std::move(sync_password_data)));
}

void PasswordStore::ClearSyncPasswordHash() {
  hash_password_manager_.ClearSavedPasswordHash();
  ScheduleTask(base::Bind(&PasswordStore::ClearSyncPasswordHashImpl, this));
}

void PasswordStore::SetPasswordStoreSigninNotifier(
    std::unique_ptr<PasswordStoreSigninNotifier> notifier) {
  DCHECK(!notifier_);
  DCHECK(notifier);
  notifier_ = std::move(notifier);
  notifier_->SubscribeToSigninEvents(this);
}
#endif

PasswordStore::~PasswordStore() {
  DCHECK(shutdown_called_);
}

scoped_refptr<base::SequencedTaskRunner>
PasswordStore::CreateBackgroundTaskRunner() const {
  return base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

void PasswordStore::InitOnBackgroundSequence(
    const syncer::SyncableService::StartSyncFlare& flare) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!syncable_service_);
  syncable_service_.reset(new PasswordSyncableService(this));
  syncable_service_->InjectStartSyncFlare(flare);
// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  reuse_detector_ = new PasswordReuseDetector;
  GetAutofillableLoginsImpl(
      std::make_unique<GetLoginsRequest>(reuse_detector_));
#endif
}

void PasswordStore::GetLoginsImpl(const FormDigest& form,
                                  std::unique_ptr<GetLoginsRequest> request) {
  request->NotifyConsumerWithResults(FillMatchingLogins(form));
}


void PasswordStore::LogStatsForBulkDeletion(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedByBulkDelete",
                       num_deletions);
}

void PasswordStore::LogStatsForBulkDeletionDuringRollback(int num_deletions) {
  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsDeletedDuringRollback",
                       num_deletions);
}

PasswordStoreChangeList PasswordStore::AddLoginSync(const PasswordForm& form) {
  // There is no good way to check if the password is actually up to date, or
  // at least to check if it was actually changed. Assume it is.
  if (AffiliatedMatchHelper::IsValidAndroidCredential(
          PasswordStore::FormDigest(form)))
    ScheduleFindAndUpdateAffiliatedWebLogins(form);
  return AddLoginImpl(form);
}

PasswordStoreChangeList PasswordStore::UpdateLoginSync(
    const PasswordForm& form) {
  if (AffiliatedMatchHelper::IsValidAndroidCredential(
          PasswordStore::FormDigest(form))) {
    // Ideally, a |form| would not be updated in any way unless it was ensured
    // that it, as a whole, can be used for a successful login. This, sadly, can
    // not be guaranteed. It might be that |form| just contains updates to some
    // meta-attribute, while it still has an out-of-date password. If such a
    // password were to be propagated to affiliated credentials in that case, it
    // may very well overwrite the actual, up-to-date password. Try to mitigate
    // this risk by ignoring updates unless they actually update the password.
    std::unique_ptr<PasswordForm> old_form(GetLoginImpl(form));
    if (old_form && form.password_value != old_form->password_value)
      ScheduleFindAndUpdateAffiliatedWebLogins(form);
  }
  return UpdateLoginImpl(form);
}

PasswordStoreChangeList PasswordStore::RemoveLoginSync(
    const PasswordForm& form) {
  return RemoveLoginImpl(form);
}

void PasswordStore::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!changes.empty()) {
    observers_->Notify(FROM_HERE, &Observer::OnLoginsChanged, changes);
    if (syncable_service_)
      syncable_service_->ActOnPasswordStoreChanges(changes);
// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    if (reuse_detector_)
      reuse_detector_->OnLoginsChanged(changes);
#endif
  }
}

// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
void PasswordStore::CheckReuseImpl(std::unique_ptr<CheckReuseRequest> request,
                                   const base::string16& input,
                                   const std::string& domain) {
  if (reuse_detector_)
    reuse_detector_->CheckReuse(input, domain, request.get());
}

void PasswordStore::SaveSyncPasswordHashImpl(
    base::Optional<SyncPasswordData> sync_password_data) {
  if (reuse_detector_)
    reuse_detector_->UseSyncPasswordHash(std::move(sync_password_data));
}

void PasswordStore::ClearSyncPasswordHashImpl() {
  if (reuse_detector_)
    reuse_detector_->ClearSyncPasswordHash();
}
#endif

void PasswordStore::Schedule(
    void (PasswordStore::*func)(std::unique_ptr<GetLoginsRequest>),
    PasswordStoreConsumer* consumer) {
  std::unique_ptr<GetLoginsRequest> request(new GetLoginsRequest(consumer));
  consumer->cancelable_task_tracker()->PostTask(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(func, this, base::Passed(&request)));
}

void PasswordStore::WrapModificationTask(ModificationTask task) {
  PasswordStoreChangeList changes = task.Run();
  NotifyLoginsChanged(changes);
}

void PasswordStore::AddLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = AddLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::UpdateLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = UpdateLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveLoginInternal(const PasswordForm& form) {
  PasswordStoreChangeList changes = RemoveLoginImpl(form);
  NotifyLoginsChanged(changes);
}

void PasswordStore::UpdateLoginWithPrimaryKeyInternal(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key) {
  PasswordStoreChangeList all_changes = RemoveLoginImpl(old_primary_key);
  PasswordStoreChangeList changes = AddLoginImpl(new_form);
  all_changes.insert(all_changes.end(), changes.begin(), changes.end());
  NotifyLoginsChanged(all_changes);
}

void PasswordStore::RemoveLoginsByURLAndTimeInternal(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  PasswordStoreChangeList changes =
      RemoveLoginsByURLAndTimeImpl(url_filter, delete_begin, delete_end);
  NotifyLoginsChanged(changes);
  if (!completion.is_null())
    main_task_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::RemoveLoginsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  PasswordStoreChangeList changes =
      RemoveLoginsCreatedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
  if (!completion.is_null())
    main_task_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::RemoveLoginsSyncedBetweenInternal(base::Time delete_begin,
                                                      base::Time delete_end) {
  PasswordStoreChangeList changes =
      RemoveLoginsSyncedBetweenImpl(delete_begin, delete_end);
  NotifyLoginsChanged(changes);
}

void PasswordStore::RemoveStatisticsByOriginAndTimeInternal(
    const base::Callback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& completion) {
  RemoveStatisticsByOriginAndTimeImpl(origin_filter, delete_begin, delete_end);
  if (!completion.is_null())
    main_task_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::DisableAutoSignInForOriginsInternal(
    const base::Callback<bool(const GURL&)>& origin_filter,
    const base::Closure& completion) {
  DisableAutoSignInForOriginsImpl(origin_filter);
  if (!completion.is_null())
    main_task_runner_->PostTask(FROM_HERE, completion);
}

void PasswordStore::GetLoginsForSameOrganizationNameImpl(
    const std::string& signon_realm,
    std::unique_ptr<GetLoginsRequest> request) {
  request->NotifyConsumerWithResults(
      FillLoginsForSameOrganizationName(signon_realm));
}

void PasswordStore::GetAutofillableLoginsImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  if (!FillAutofillableLogins(&obtained_forms))
    obtained_forms.clear();
  request->NotifyConsumerWithResults(std::move(obtained_forms));
}

void PasswordStore::
    GetAutofillableLoginsWithAffiliationAndBrandingInformationImpl(
        std::unique_ptr<GetLoginsRequest> request) {
  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  if (!FillAutofillableLogins(&obtained_forms))
    obtained_forms.clear();
  // Since AffiliatedMatchHelper's requests should be sent from UI thread,
  // post a request to UI thread.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PasswordStore::InjectAffiliationAndBrandingInformation, this,
                 base::Passed(&obtained_forms), base::Passed(&request)));
}

void PasswordStore::GetBlacklistLoginsImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  if (!FillBlacklistLogins(&obtained_forms))
    obtained_forms.clear();
  request->NotifyConsumerWithResults(std::move(obtained_forms));
}

void PasswordStore::GetBlacklistLoginsWithAffiliationAndBrandingInformationImpl(
    std::unique_ptr<GetLoginsRequest> request) {
  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  if (!FillBlacklistLogins(&obtained_forms))
    obtained_forms.clear();
  // Since AffiliatedMatchHelper's requests should be sent from UI thread,
  // post a request to UI thread.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PasswordStore::InjectAffiliationAndBrandingInformation, this,
                 base::Passed(&obtained_forms), base::Passed(&request)));
}

void PasswordStore::NotifyAllSiteStats(
    std::unique_ptr<GetLoginsRequest> request) {
  request->NotifyWithSiteStatistics(GetAllSiteStatsImpl());
}

void PasswordStore::NotifySiteStats(const GURL& origin_domain,
                                    std::unique_ptr<GetLoginsRequest> request) {
  request->NotifyWithSiteStatistics(GetSiteStatsImpl(origin_domain));
}

void PasswordStore::GetLoginsWithAffiliationsImpl(
    const FormDigest& form,
    std::unique_ptr<GetLoginsRequest> request,
    const std::vector<std::string>& additional_android_realms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<PasswordForm>> results(FillMatchingLogins(form));
  for (const std::string& realm : additional_android_realms) {
    std::vector<std::unique_ptr<PasswordForm>> more_results(
        FillMatchingLogins({PasswordForm::SCHEME_HTML, realm, GURL()}));
    for (auto& result : more_results)
      result->is_affiliation_based_match = true;
    password_manager_util::TrimUsernameOnlyCredentials(&more_results);
    const size_t results_count = results.size();
    results.resize(results_count + more_results.size());
    std::move(more_results.begin(), more_results.end(),
              results.begin() + results_count);
  }
  request->NotifyConsumerWithResults(std::move(results));
}

void PasswordStore::InjectAffiliationAndBrandingInformation(
    std::vector<std::unique_ptr<PasswordForm>> forms,
    std::unique_ptr<GetLoginsRequest> request) {
  if (affiliated_match_helper_) {
    affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
        std::move(forms),
        base::Bind(&PasswordStore::GetLoginsRequest::NotifyConsumerWithResults,
                   base::Owned(request.release())));
  } else {
    request->NotifyConsumerWithResults(std::move(forms));
  }
}

void PasswordStore::ScheduleGetLoginsWithAffiliations(
    const FormDigest& form,
    std::unique_ptr<GetLoginsRequest> request,
    const std::vector<std::string>& additional_android_realms) {
  ScheduleTask(base::Bind(&PasswordStore::GetLoginsWithAffiliationsImpl, this,
                          form, base::Passed(&request),
                          additional_android_realms));
}

std::unique_ptr<PasswordForm> PasswordStore::GetLoginImpl(
    const PasswordForm& primary_key) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<PasswordForm>> candidates(
      FillMatchingLogins(FormDigest(primary_key)));
  for (auto& candidate : candidates) {
    if (ArePasswordFormUniqueKeyEqual(*candidate, primary_key) &&
        !candidate->is_public_suffix_match) {
      return std::move(candidate);
    }
  }
  return nullptr;
}

void PasswordStore::FindAndUpdateAffiliatedWebLogins(
    const PasswordForm& added_or_updated_android_form) {
  if (!affiliated_match_helper_ ||
      !is_propagating_password_changes_to_web_credentials_enabled_) {
    return;
  }
  affiliated_match_helper_->GetAffiliatedWebRealms(
      PasswordStore::FormDigest(added_or_updated_android_form),
      base::Bind(&PasswordStore::ScheduleUpdateAffiliatedWebLoginsImpl, this,
                 added_or_updated_android_form));
}

void PasswordStore::ScheduleFindAndUpdateAffiliatedWebLogins(
    const PasswordForm& added_or_updated_android_form) {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PasswordStore::FindAndUpdateAffiliatedWebLogins,
                            this, added_or_updated_android_form));
}

void PasswordStore::UpdateAffiliatedWebLoginsImpl(
    const PasswordForm& updated_android_form,
    const std::vector<std::string>& affiliated_web_realms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  PasswordStoreChangeList all_changes;
  for (const std::string& affiliated_web_realm : affiliated_web_realms) {
    std::vector<std::unique_ptr<PasswordForm>> web_logins(FillMatchingLogins(
        {PasswordForm::SCHEME_HTML, affiliated_web_realm, GURL()}));
    for (auto& web_login : web_logins) {
      // Do not update HTTP logins, logins saved under insecure conditions, and
      // non-HTML login forms; PSL matches; logins with a different username;
      // and logins with the same password (to avoid generating no-op updates).
      if (!AffiliatedMatchHelper::IsValidWebCredential(
              FormDigest(*web_login)) ||
          web_login->is_public_suffix_match ||
          web_login->username_value != updated_android_form.username_value ||
          web_login->password_value == updated_android_form.password_value)
        continue;

      // If the |web_login| was updated in the same or a later chunk of Sync
      // changes, assume that it is more recent and do not update it. Note that
      // this check is far from perfect conflict resolution and mostly prevents
      // long-dormant Sync clients doing damage when they wake up in the face
      // of the following list of changes:
      //
      //   Time   Source     Change
      //   ====   ======     ======
      //   #1     Android    android_login.password_value = "A"
      //   #2     Client A   web_login.password_value = "A" (propagation)
      //   #3     Client A   web_login.password_value = "B" (manual overwrite)
      //
      // When long-dormant Sync client B wakes up, it will only get a distilled
      // subset of not-yet-obsoleted changes {1, 3}. In this case, client B must
      // not propagate password "A" to |web_login|. This is prevented as change
      // #3 will arrive either in the same/later chunk of sync changes, so the
      // |date_synced| of |web_login| value will be greater or equal.
      //
      // Note that this solution has several shortcomings:
      //
      //   (1) It will not prevent local changes to |web_login| from being
      //       overwritten if they were made shortly after start-up, before
      //       Sync changes are applied. This should be tolerable.
      //
      //   (2) It assumes that all Sync clients are fully capable of propagating
      //       changes to web credentials on their own. If client C runs an
      //       older version of Chrome and updates the password for |web_login|
      //       around the time when the |android_login| is updated, the updated
      //       password will not be propagated by client B to |web_login| when
      //       it wakes up, regardless of the temporal order of the original
      //       changes, as client B will see both credentials having the same
      //       |data_synced|.
      //
      //   (2a) Above could be mitigated by looking not only at |data_synced|,
      //        but also at the actual order of Sync changes.
      //
      //   (2b) However, (2a) is still not workable, as a Sync change is made
      //        when any attribute of the credential is updated, not only the
      //        password. Hence it is not possible for client B to distinguish
      //        between two following two event orders:
      //
      //    #1     Android    android_login.password_value = "A"
      //    #2     Client C   web_login.password_value = "B" (manual overwrite)
      //    #3     Android    android_login.random_attribute = "..."
      //
      //    #1     Client C   web_login.password_value = "B" (manual overwrite)
      //    #2     Android    android_login.password_value = "A"
      //
      //        And so it must assume that it is unsafe to update |web_login|.
      if (web_login->date_synced >= updated_android_form.date_synced)
        continue;

      web_login->password_value = updated_android_form.password_value;

      PasswordStoreChangeList changes = UpdateLoginImpl(*web_login);
      all_changes.insert(all_changes.end(), changes.begin(), changes.end());
    }
  }
  NotifyLoginsChanged(all_changes);
}

void PasswordStore::ScheduleUpdateAffiliatedWebLoginsImpl(
    const PasswordForm& updated_android_form,
    const std::vector<std::string>& affiliated_web_realms) {
  ScheduleTask(base::Bind(&PasswordStore::UpdateAffiliatedWebLoginsImpl, this,
                          updated_android_form, affiliated_web_realms));
}

void PasswordStore::DestroyOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  syncable_service_.reset();
// TODO(crbug.com/706392): Fix password reuse detection for Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  delete reuse_detector_;
  reuse_detector_ = nullptr;
#endif
}

std::ostream& operator<<(std::ostream& os,
                         const PasswordStore::FormDigest& digest) {
  return os << "FormDigest(scheme: " << digest.scheme
            << ", signon_realm: " << digest.signon_realm
            << ", origin: " << digest.origin << ")";
}

}  // namespace password_manager
