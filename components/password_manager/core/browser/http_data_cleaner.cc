// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_data_cleaner.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

constexpr int kDefaultDelay = 40;

// Utility function that moves all elements from a specified iterator into a new
// vector and returns it. The moved elements are deleted from the original
// vector.
std::vector<std::unique_ptr<PasswordForm>> SplitFormsFrom(
    std::vector<std::unique_ptr<PasswordForm>>::iterator from,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  const auto end_forms = std::end(*forms);
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.reserve(std::distance(from, end_forms));
  std::move(from, end_forms, std::back_inserter(result));
  forms->erase(from, end_forms);
  return result;
}

void RemoveLoginIfHSTS(const scoped_refptr<PasswordStore>& store,
                       const PasswordForm& form,
                       bool is_hsts) {
  if (is_hsts)
    store->RemoveLogin(form);
}

void RemoveSiteStatsIfHSTS(const scoped_refptr<PasswordStore>& store,
                           const InteractionsStats& stats,
                           bool is_hsts) {
  if (is_hsts)
    store->RemoveSiteStats(stats.origin_domain);
}

// This class removes obsolete HTTP data from a password store. HTTP data is
// obsolete if the corresponding host migrated to HTTPS and has HSTS enabled.
class ObsoleteHttpCleaner : public password_manager::PasswordStoreConsumer {
 public:
  // Constructing a ObsoleteHttpCleaner will result in issuing the clean up
  // tasks already.
  ObsoleteHttpCleaner(
      const scoped_refptr<PasswordStore>& store,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  ~ObsoleteHttpCleaner() override;

  // PasswordStoreConsumer:
  // This will be called for both autofillable logins as well as blacklisted
  // logins. Blacklisted logins are removed iff the scheme is HTTP and HSTS is
  // enabled for the host.
  // Autofillable logins are removed iff the scheme is HTTP and there exists
  // another HTTPS login with active HSTS that has the same host as well as the
  // same username and password.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // This will remove all stats for HTTP sites for which HSTS is active.
  void OnGetSiteStatistics(std::vector<InteractionsStats> stats) override;

  PasswordStore* store() { return store_.get(); }

  const scoped_refptr<net::URLRequestContextGetter>& request_context() {
    return request_context_;
  }

  bool finished_cleaning() const { return remaining_cleaning_tasks_ == 0; }

 private:
  scoped_refptr<PasswordStore> store_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  // There are 3 cleaning tasks initiated in the constructor.
  int remaining_cleaning_tasks_ = 3;

  DISALLOW_COPY_AND_ASSIGN(ObsoleteHttpCleaner);
};

ObsoleteHttpCleaner::ObsoleteHttpCleaner(
    const scoped_refptr<PasswordStore>& password_store,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : store_(password_store), request_context_(request_context) {
  DCHECK(store_);
  DCHECK(request_context_.get());
  store()->GetBlacklistLogins(this);
  store()->GetAutofillableLogins(this);
  store()->GetAllSiteStats(this);
}

ObsoleteHttpCleaner::~ObsoleteHttpCleaner() = default;

void ObsoleteHttpCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  --remaining_cleaning_tasks_;
  // Non HTTP or HTTPS credentials are ignored.
  base::EraseIf(results, [](const std::unique_ptr<PasswordForm>& form) {
    return !form->origin.SchemeIsHTTPOrHTTPS();
  });

  // Move HTTPS forms into their own container.
  auto https_forms = SplitFormsFrom(
      std::partition(std::begin(results), std::end(results),
                     [](const std::unique_ptr<PasswordForm>& form) {
                       return form->origin.SchemeIs(url::kHttpScheme);
                     }),
      &results);

  // Move blacklisted HTTP forms into their own container.
  const auto blacklisted_http_forms = SplitFormsFrom(
      std::partition(std::begin(results), std::end(results),
                     [](const std::unique_ptr<PasswordForm>& form) {
                       return !form->blacklisted_by_user;
                     }),
      &results);

  // Remove blacklisted HTTP forms from the password store when HSTS is active
  // for the given host.
  for (const auto& form : blacklisted_http_forms) {
    PostHSTSQueryForHostAndRequestContext(
        form->origin, request_context(),
        base::Bind(&RemoveLoginIfHSTS, base::WrapRefCounted(store()), *form));
  }

  // Return early if there are no non-blacklisted HTTP forms.
  if (results.empty())
    return;

  // Sort HTTPS forms according to custom comparison function. Consider two
  // forms equivalent if they have the same host, as well as the same username
  // and password.
  const auto form_cmp = [](const std::unique_ptr<PasswordForm>& lhs,
                           const std::unique_ptr<PasswordForm>& rhs) {
    return std::forward_as_tuple(lhs->origin.host_piece(), lhs->username_value,
                                 lhs->password_value) <
           std::forward_as_tuple(rhs->origin.host_piece(), rhs->username_value,
                                 rhs->password_value);
  };

  std::sort(std::begin(https_forms), std::end(https_forms), form_cmp);

  // Iterate through HTTP forms and remove them from the password store if there
  // exists an equivalent HTTPS form that has HSTS enabled.
  for (const auto& form : results) {
    if (std::binary_search(std::begin(https_forms), std::end(https_forms), form,
                           form_cmp)) {
      PostHSTSQueryForHostAndRequestContext(
          form->origin, request_context(),
          base::Bind(&RemoveLoginIfHSTS, base::WrapRefCounted(store()), *form));
    }
  }
}

void ObsoleteHttpCleaner::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {
  --remaining_cleaning_tasks_;
  for (const auto& stat : stats) {
    if (stat.origin_domain.SchemeIs(url::kHttpScheme)) {
      PostHSTSQueryForHostAndRequestContext(
          stat.origin_domain, request_context(),
          base::Bind(&RemoveSiteStatsIfHSTS, base::WrapRefCounted(store()),
                     stat));
    }
  }
}

void WaitUntilCleaningIsDone(std::unique_ptr<ObsoleteHttpCleaner> cleaner,
                             PrefService* prefs) {
  // Given the async nature of the cleaning tasks it is non-trivial to determine
  // when they are all done. In this method we make use of the fact that as long
  // the cleaning is not completed, weak pointers to the cleaner object exist
  // (the scheduled, but not yet executed tasks hold them). If the cleaning is
  // not done yet, this method schedules a task on the password store, which
  // will be behind the cleaning tasks in the task queue. When the scheduled
  // task gets executed, this method is called again. Now it is guaranteed that
  // the initial scheduled cleaning tasks will have been executed, but it might
  // be the case that they wait for the result of other scheduled tasks (e.g. on
  // Windows there could be async calls to GetIE7Login). In this case another
  // round trip of tasks will be scheduled. Finally, when no weak ptrs remain,
  // this method sets a boolean preference flag and returns.
  if (cleaner->HasWeakPtrs()) {
    scoped_refptr<base::SequencedTaskRunner> main_thread_runner =
        base::SequencedTaskRunnerHandle::Get();
    const auto post_to_thread =
        [](std::unique_ptr<ObsoleteHttpCleaner> cleaner, PrefService* prefs,
           scoped_refptr<base::SequencedTaskRunner> thread_runner) {
          thread_runner->PostTask(
              FROM_HERE, base::Bind(&WaitUntilCleaningIsDone,
                                    base::Passed(std::move(cleaner)), prefs));
        };

    // Calling |ScheduleTask| through the raw pointer is necessary, because
    // |std::move(cleaner)| might be executed before |cleaner->store()|.
    // However, at this point cleaner is moved from, leading to a crash. Using a
    // raw pointer avoids this issue.
    ObsoleteHttpCleaner* raw_cleaner = cleaner.get();
    raw_cleaner->store()->ScheduleTask(
        base::Bind(post_to_thread, base::Passed(std::move(cleaner)), prefs,
                   main_thread_runner));
    return;
  }

  DCHECK(cleaner->finished_cleaning());
  prefs->SetBoolean(password_manager::prefs::kWasObsoleteHttpDataCleaned, true);
}

void InitiateCleaning(
    const scoped_refptr<PasswordStore>& store,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context) {
  WaitUntilCleaningIsDone(
      std::make_unique<ObsoleteHttpCleaner>(store, request_context), prefs);
}

void DelayCleanObsoleteHttpDataForPasswordStoreAndPrefsImpl(
    PasswordStore* store,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    int delay_in_seconds) {
  if (!prefs->GetBoolean(
          password_manager::prefs::kWasObsoleteHttpDataCleaned)) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&InitiateCleaning, base::WrapRefCounted(store), prefs,
                   request_context),
        base::TimeDelta::FromSeconds(delay_in_seconds));
  }
}

}  // namespace

void DelayCleanObsoleteHttpDataForPasswordStoreAndPrefs(
    PasswordStore* store,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context) {
  DelayCleanObsoleteHttpDataForPasswordStoreAndPrefsImpl(
      store, prefs, request_context, kDefaultDelay);
}

void CleanObsoleteHttpDataForPasswordStoreAndPrefsForTesting(
    PasswordStore* store,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context) {
  DelayCleanObsoleteHttpDataForPasswordStoreAndPrefsImpl(store, prefs,
                                                         request_context, 0);
}

}  // namespace password_manager
