// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/language/url_language_histogram_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/net/http_cache_helper.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;
using web::WebThread;

namespace {

using CallbackList = base::CallbackList<void(
    const IOSChromeBrowsingDataRemover::NotificationDetails&)>;

// Contains all registered callbacks for browsing data removed notifications.
CallbackList* g_on_browsing_data_removed_callbacks = nullptr;

// Accessor for |*g_on_browsing_data_removed_callbacks|. Creates a new object
// the first time so that it always returns a valid object.
CallbackList* GetOnBrowsingDataRemovedCallbacks() {
  if (!g_on_browsing_data_removed_callbacks)
    g_on_browsing_data_removed_callbacks = new CallbackList();
  return g_on_browsing_data_removed_callbacks;
}

bool AllDomainsPredicate(const std::string& domain) {
  return true;
}

}  // namespace

bool IOSChromeBrowsingDataRemover::is_removing_ = false;

IOSChromeBrowsingDataRemover::NotificationDetails::NotificationDetails()
    : removal_begin(base::Time()), removal_mask(-1) {}

IOSChromeBrowsingDataRemover::NotificationDetails::NotificationDetails(
    const IOSChromeBrowsingDataRemover::NotificationDetails& details)
    : removal_begin(details.removal_begin),
      removal_mask(details.removal_mask) {}

IOSChromeBrowsingDataRemover::NotificationDetails::NotificationDetails(
    base::Time removal_begin,
    int removal_mask)
    : removal_begin(removal_begin), removal_mask(removal_mask) {}

IOSChromeBrowsingDataRemover::NotificationDetails::~NotificationDetails() {}

// Static.
IOSChromeBrowsingDataRemover* IOSChromeBrowsingDataRemover::CreateForPeriod(
    ios::ChromeBrowserState* browser_state,
    browsing_data::TimePeriod period) {
  switch (period) {
    case browsing_data::TimePeriod::LAST_HOUR:
      base::RecordAction(UserMetricsAction("ClearBrowsingData_LastHour"));
      break;
    case browsing_data::TimePeriod::LAST_DAY:
      base::RecordAction(UserMetricsAction("ClearBrowsingData_LastDay"));
      break;
    case browsing_data::TimePeriod::LAST_WEEK:
      base::RecordAction(UserMetricsAction("ClearBrowsingData_LastWeek"));
      break;
    case browsing_data::TimePeriod::FOUR_WEEKS:
      base::RecordAction(UserMetricsAction("ClearBrowsingData_LastMonth"));
      break;
    case browsing_data::TimePeriod::ALL_TIME:
      base::RecordAction(UserMetricsAction("ClearBrowsingData_Everything"));
      break;
  }
  return new IOSChromeBrowsingDataRemover(
      browser_state, browsing_data::CalculateBeginDeleteTime(period),
      base::Time::Max());
}

IOSChromeBrowsingDataRemover::IOSChromeBrowsingDataRemover(
    ios::ChromeBrowserState* browser_state,
    base::Time delete_begin,
    base::Time delete_end)
    : browser_state_(browser_state),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      main_context_getter_(browser_state->GetRequestContext()) {
  DCHECK(browser_state);
  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max(). Work around
  // it here. New code should use base::Time::Max().
  DCHECK(delete_end_ != base::Time());
  if (delete_end_ == base::Time())
    delete_end_ = base::Time::Max();
}

IOSChromeBrowsingDataRemover::~IOSChromeBrowsingDataRemover() {
  DCHECK(AllDone());
}

// Static.
void IOSChromeBrowsingDataRemover::set_removing(bool is_removing) {
  DCHECK(is_removing_ != is_removing);
  is_removing_ = is_removing;
}

void IOSChromeBrowsingDataRemover::Remove(int remove_mask) {
  RemoveImpl(remove_mask);
}

void IOSChromeBrowsingDataRemover::RemoveImpl(int remove_mask) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  set_removing(true);
  remove_mask_ = remove_mask;

  // On other platforms, it is possible to specify different types of origins
  // to clear data for (e.g., unprotected web vs. extensions). On iOS, this
  // mask is always implicitly the unprotected web, which is the only type that
  // is relevant. This metric is left here for historical consistency.
  base::RecordAction(
      UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));

  if (remove_mask & REMOVE_HISTORY) {
    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      std::set<GURL> restrict_urls;
      base::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      waiting_for_clear_history_ = true;

      history_service->ExpireLocalAndRemoteHistoryBetween(
          ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_),
          restrict_urls, delete_begin_, delete_end_,
          base::Bind(&IOSChromeBrowsingDataRemover::OnHistoryDeletionDone,
                     base::Unretained(this)),
          &history_task_tracker_);
    }

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history: we have no mechanism to track when these items were
    // created, so we'll clear them all. Better safe than sorry.
    IOSChromeIOThread* ios_chrome_io_thread =
        GetApplicationContext()->GetIOSChromeIOThread();
    if (ios_chrome_io_thread) {
      waiting_for_clear_hostname_resolution_cache_ = true;
      web::WebThread::PostTaskAndReply(
          web::WebThread::IO, FROM_HERE,
          base::Bind(&IOSChromeIOThread::ClearHostCache,
                     base::Unretained(ios_chrome_io_thread)),
          base::Bind(
              &IOSChromeBrowsingDataRemover::OnClearedHostnameResolutionCache,
              base::Unretained(this)));
    }

    // As part of history deletion we also delete the auto-generated keywords.
    // Because the TemplateURLService is shared between incognito and
    // non-incognito profiles, don't do this in incognito.
    if (!browser_state_->IsOffTheRecord()) {
      TemplateURLService* keywords_model =
          ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
      if (keywords_model && !keywords_model->loaded()) {
        template_url_sub_ = keywords_model->RegisterOnLoadedCallback(
            base::Bind(&IOSChromeBrowsingDataRemover::OnKeywordsLoaded,
                       base::Unretained(this)));
        keywords_model->Load();
        waiting_for_clear_keyword_data_ = true;
      } else if (keywords_model) {
        keywords_model->RemoveAutoGeneratedBetween(delete_begin_, delete_end_);
      }
    }

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    // We also delete the list of recently closed tabs. Since these expire,
    // they can't be more than a day old, so we can simply clear them all.
    sessions::TabRestoreService* tab_service =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state_);
    if (tab_service) {
      tab_service->ClearEntries();
      tab_service->DeleteLastSession();
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      waiting_for_clear_autofill_origin_urls_ = true;
      web_data_service->RemoveOriginURLsModifiedBetween(delete_begin_,
                                                        delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      WebThread::PostTaskAndReply(
          WebThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&IOSChromeBrowsingDataRemover::OnClearedAutofillOriginURLs,
                     base::Unretained(this)));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForBrowserState(
              browser_state_);
      if (data_manager)
        data_manager->Refresh();
    }

    // Remove language histogram history.
    language::UrlLanguageHistogram* language_histogram =
        UrlLanguageHistogramFactory::GetForBrowserState(browser_state_);
    if (language_histogram) {
      language_histogram->ClearHistory(delete_begin_, delete_end_);
    }
  }

  if (remove_mask & REMOVE_COOKIES) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));

    ++waiting_for_clear_cookies_count_;
    WebThread::PostTask(
        WebThread::IO, FROM_HERE,
        base::Bind(&IOSChromeBrowsingDataRemover::ClearCookiesOnIOThread,
                   base::Unretained(this), main_context_getter_));

    // TODO(mkwst): If we're not removing passwords, then clear the 'zero-click'
    // flag for all credentials in the password store.
  }

  if (remove_mask & REMOVE_CHANNEL_IDS) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ChannelIDs"));
    if (main_context_getter_) {
      waiting_for_clear_channel_ids_ = true;
      WebThread::PostTask(
          WebThread::IO, FROM_HERE,
          base::Bind(&IOSChromeBrowsingDataRemover::ClearChannelIDsOnIOThread,
                     base::Unretained(this), main_context_getter_));
    }
  }

  if (remove_mask & REMOVE_PASSWORDS) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStore* password_store =
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store) {
      waiting_for_clear_passwords_ = true;
      password_store->RemoveLoginsCreatedBetween(
          delete_begin_, delete_end_,
          base::Bind(&IOSChromeBrowsingDataRemover::OnClearedPasswords,
                     base::Unretained(this)));
    }
  }

  if (remove_mask & REMOVE_FORM_DATA) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
            browser_state_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      waiting_for_clear_form_ = true;
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
                                                       delete_end_);
      web_data_service->RemoveAutofillDataModifiedBetween(delete_begin_,
                                                          delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      WebThread::PostTaskAndReply(
          WebThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&IOSChromeBrowsingDataRemover::OnClearedFormData,
                     base::Unretained(this)));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForBrowserState(
              browser_state_);
      if (data_manager)
        data_manager->Refresh();
    }
  }

  if (remove_mask & REMOVE_CACHE) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    waiting_for_clear_cache_ = true;
    ClearHttpCache(browser_state_->GetRequestContext(),
                   WebThread::GetTaskRunnerForThread(WebThread::IO),
                   delete_begin_, delete_end_,
                   base::Bind(&IOSChromeBrowsingDataRemover::OnClearedCache,
                              base::Unretained(this)));
  }

  // Remove omnibox zero-suggest cache results.
  if ((remove_mask & (REMOVE_CACHE | REMOVE_COOKIES))) {
    browser_state_->GetPrefs()->SetString(omnibox::kZeroSuggestCachedResults,
                                          std::string());
  }

  // Always wipe accumulated network related data (TransportSecurityState and
  // HttpServerPropertiesManager data).
  waiting_for_clear_networking_history_ = true;
  browser_state_->ClearNetworkingHistorySince(
      delete_begin_,
      base::Bind(&IOSChromeBrowsingDataRemover::OnClearedNetworkingHistory,
                 base::Unretained(this)));

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice = NEITHER_COOKIES_NOR_CACHE;
  if (remove_mask & REMOVE_COOKIES) {
    choice = remove_mask & REMOVE_CACHE ? BOTH_COOKIES_AND_CACHE : ONLY_COOKIES;
  } else if (remove_mask & REMOVE_CACHE) {
    choice = ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache", choice,
      MAX_CHOICE_VALUE);
}

void IOSChromeBrowsingDataRemover::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IOSChromeBrowsingDataRemover::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IOSChromeBrowsingDataRemover::OnHistoryDeletionDone() {
  waiting_for_clear_history_ = false;
  NotifyAndDeleteIfDone();
}

bool IOSChromeBrowsingDataRemover::AllDone() {
  return !waiting_for_clear_autofill_origin_urls_ &&
         !waiting_for_clear_cache_ && !waiting_for_clear_channel_ids_ &&
         !waiting_for_clear_cookies_count_ && !waiting_for_clear_form_ &&
         !waiting_for_clear_history_ &&
         !waiting_for_clear_hostname_resolution_cache_ &&
         !waiting_for_clear_keyword_data_ &&
         !waiting_for_clear_networking_history_ &&
         !waiting_for_clear_passwords_;
}

void IOSChromeBrowsingDataRemover::OnKeywordsLoaded() {
  // Deletes the entries from the model, and if we're not waiting on anything
  // else notifies observers and deletes this IOSChromeBrowsingDataRemover.
  TemplateURLService* model =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
  model->RemoveAutoGeneratedBetween(delete_begin_, delete_end_);
  waiting_for_clear_keyword_data_ = false;
  template_url_sub_.reset();
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::NotifyAndDelete() {
  set_removing(false);

  // Notify observers.
  IOSChromeBrowsingDataRemover::NotificationDetails details(delete_begin_,
                                                            remove_mask_);

  GetOnBrowsingDataRemovedCallbacks()->Notify(details);

  for (auto& observer : observer_list_)
    observer.OnIOSChromeBrowsingDataRemoverDone();

  // History requests aren't happy if you delete yourself from the callback.
  // As such, we do a delete later.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void IOSChromeBrowsingDataRemover::NotifyAndDeleteIfDone() {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.

  if (!AllDone())
    return;

  NotifyAndDelete();
}

void IOSChromeBrowsingDataRemover::OnClearedHostnameResolutionCache() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  waiting_for_clear_hostname_resolution_cache_ = false;
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::OnClearedNetworkingHistory() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  waiting_for_clear_networking_history_ = false;
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::OnClearedCache(int error) {
  waiting_for_clear_cache_ = false;

  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::OnClearedPasswords() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  waiting_for_clear_passwords_ = false;
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::OnClearedCookies(uint32_t num_deleted) {
  if (!WebThread::CurrentlyOn(WebThread::UI)) {
    WebThread::PostTask(
        WebThread::UI, FROM_HERE,
        base::Bind(&IOSChromeBrowsingDataRemover::OnClearedCookies,
                   base::Unretained(this), num_deleted));
    return;
  }

  DCHECK_GT(waiting_for_clear_cookies_count_, 0);
  --waiting_for_clear_cookies_count_;
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::ClearCookiesOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& rq_context) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  net::CookieStore* cookie_store =
      rq_context->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenAsync(
      delete_begin_, delete_end_,
      base::Bind(&IOSChromeBrowsingDataRemover::OnClearedCookies,
                 base::Unretained(this)));
}

void IOSChromeBrowsingDataRemover::ClearChannelIDsOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& rq_context) {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  net::ChannelIDService* channel_id_service =
      rq_context->GetURLRequestContext()->channel_id_service();
  channel_id_service->GetChannelIDStore()->DeleteForDomainsCreatedBetween(
      base::Bind(&AllDomainsPredicate), delete_begin_, delete_end_,
      base::Bind(&IOSChromeBrowsingDataRemover::OnClearedChannelIDsOnIOThread,
                 base::Unretained(this), rq_context));
}

void IOSChromeBrowsingDataRemover::OnClearedChannelIDsOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& rq_context) {
  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  rq_context->GetURLRequestContext()
      ->ssl_config_service()
      ->NotifySSLConfigChange();
  WebThread::PostTask(
      WebThread::UI, FROM_HERE,
      base::Bind(&IOSChromeBrowsingDataRemover::OnClearedChannelIDs,
                 base::Unretained(this)));
}

void IOSChromeBrowsingDataRemover::OnClearedChannelIDs() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  waiting_for_clear_channel_ids_ = false;
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::OnClearedFormData() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  waiting_for_clear_form_ = false;
  NotifyAndDeleteIfDone();
}

void IOSChromeBrowsingDataRemover::OnClearedAutofillOriginURLs() {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  waiting_for_clear_autofill_origin_urls_ = false;
  NotifyAndDeleteIfDone();
}

// static
IOSChromeBrowsingDataRemover::CallbackSubscription
IOSChromeBrowsingDataRemover::RegisterOnBrowsingDataRemovedCallback(
    const IOSChromeBrowsingDataRemover::Callback& callback) {
  return GetOnBrowsingDataRemovedCallbacks()->Add(callback);
}
