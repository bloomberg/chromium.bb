// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "net/base/net_errors.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/base/transport_security_state.h"
#include "net/cookies/cookie_store.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"
#include "webkit/quota/special_storage_policy.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;
using content::DownloadManager;
using content::UserMetricsAction;

bool BrowsingDataRemover::is_removing_ = false;

BrowsingDataRemover::NotificationDetails::NotificationDetails()
    : removal_begin(base::Time()),
      removal_mask(-1),
      origin_set_mask(-1) {
}

BrowsingDataRemover::NotificationDetails::NotificationDetails(
    const BrowsingDataRemover::NotificationDetails& details)
    : removal_begin(details.removal_begin),
      removal_mask(details.removal_mask),
      origin_set_mask(details.origin_set_mask) {
}

BrowsingDataRemover::NotificationDetails::NotificationDetails(
    base::Time removal_begin,
    int removal_mask,
    int origin_set_mask)
    : removal_begin(removal_begin),
      removal_mask(removal_mask),
      origin_set_mask(origin_set_mask) {
}

BrowsingDataRemover::NotificationDetails::~NotificationDetails() {}

// Static.
BrowsingDataRemover* BrowsingDataRemover::CreateForUnboundedRange(
    Profile* profile) {
  return new BrowsingDataRemover(profile, base::Time(), base::Time::Max());
}

// Static.
BrowsingDataRemover* BrowsingDataRemover::CreateForRange(Profile* profile,
    base::Time start, base::Time end) {
  return new BrowsingDataRemover(profile, start, end);
}

// Static.
BrowsingDataRemover* BrowsingDataRemover::CreateForPeriod(Profile* profile,
    TimePeriod period) {
  return new BrowsingDataRemover(profile,
      BrowsingDataRemover::CalculateBeginDeleteTime(period),
      base::Time::Max());
}

BrowsingDataRemover::BrowsingDataRemover(Profile* profile,
                                         base::Time delete_begin,
                                         base::Time delete_end)
    : profile_(profile),
      quota_manager_(NULL),
      dom_storage_context_(NULL),
      special_storage_policy_(profile->GetExtensionSpecialStoragePolicy()),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      next_cache_state_(STATE_NONE),
      cache_(NULL),
      main_context_getter_(profile->GetRequestContext()),
      media_context_getter_(profile->GetMediaRequestContext()),
      deauthorize_content_licenses_request_id_(0),
      waiting_for_clear_cache_(false),
      waiting_for_clear_nacl_cache_(false),
      waiting_for_clear_cookies_count_(0),
      waiting_for_clear_history_(false),
      waiting_for_clear_local_storage_(false),
      waiting_for_clear_networking_history_(false),
      waiting_for_clear_server_bound_certs_(false),
      waiting_for_clear_plugin_data_(false),
      waiting_for_clear_quota_managed_data_(false),
      waiting_for_clear_content_licenses_(false),
      waiting_for_clear_form_(false),
      remove_mask_(0),
      remove_origin_(GURL()),
      origin_set_mask_(0) {
  DCHECK(profile);
  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max(). Work around
  // it here. New code should use base::Time::Max().
  DCHECK(delete_end_ != base::Time());
  if (delete_end_ == base::Time())
    delete_end_ = base::Time::Max();
}

BrowsingDataRemover::~BrowsingDataRemover() {
  DCHECK(AllDone());
}

// Static.
void BrowsingDataRemover::set_removing(bool is_removing) {
  DCHECK(is_removing_ != is_removing);
  is_removing_ = is_removing;
}

// Static.
int BrowsingDataRemover::GenerateQuotaClientMask(int remove_mask) {
  int quota_client_mask = 0;
  if (remove_mask & BrowsingDataRemover::REMOVE_FILE_SYSTEMS)
    quota_client_mask |= quota::QuotaClient::kFileSystem;
  if (remove_mask & BrowsingDataRemover::REMOVE_WEBSQL)
    quota_client_mask |= quota::QuotaClient::kDatabase;
  if (remove_mask & BrowsingDataRemover::REMOVE_APPCACHE)
    quota_client_mask |= quota::QuotaClient::kAppcache;
  if (remove_mask & BrowsingDataRemover::REMOVE_INDEXEDDB)
    quota_client_mask |= quota::QuotaClient::kIndexedDatabase;

  return quota_client_mask;
}

void BrowsingDataRemover::Remove(int remove_mask, int origin_set_mask) {
  RemoveImpl(remove_mask, GURL(), origin_set_mask);
}

void BrowsingDataRemover::RemoveImpl(int remove_mask,
                                     const GURL& origin,
                                     int origin_set_mask) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  set_removing(true);
  remove_mask_ = remove_mask;
  remove_origin_ = origin;
  origin_set_mask_ = origin_set_mask;

  if (origin_set_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));
  }
  if (origin_set_mask_ & BrowsingDataHelper::PROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsProtectedWeb"));
  }
  if (origin_set_mask_ & BrowsingDataHelper::EXTENSION) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsExtension"));
  }
  // If this fires, we added a new BrowsingDataHelper::OriginSetMask without
  // updating the user metrics above.
  COMPILE_ASSERT(
      BrowsingDataHelper::ALL == (BrowsingDataHelper::UNPROTECTED_WEB |
                                  BrowsingDataHelper::PROTECTED_WEB |
                                  BrowsingDataHelper::EXTENSION),
      forgotten_to_add_origin_mask_type);

  if (remove_mask & REMOVE_HISTORY) {
    HistoryService* history_service = HistoryServiceFactory::GetForProfile(
        profile_, Profile::EXPLICIT_ACCESS);
    if (history_service) {
      std::set<GURL> restrict_urls;
      if (!remove_origin_.is_empty())
        restrict_urls.insert(remove_origin_);
      content::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      waiting_for_clear_history_ = true;

      // The HistoryService special-cases an end time of base::Time() to
      // efficiently remove the whole history database. Support that here by
      // passing base::Time() into HistoryService::ExpireHistoryBetween rather
      // than base::Time::Max().
      //
      // TODO(sky?): Adjust HistoryService so that it understands Time::Max()
      //     and deals well with non-max/non-null time periods: see
      //     http://crbug.com/145680 for details.
      base::Time history_end_ = delete_end_ == base::Time::Max() ?
            base::Time() : delete_end_;
      history_service->ExpireHistoryBetween(restrict_urls,
          delete_begin_, history_end_,
          &request_consumer_,
          base::Bind(&BrowsingDataRemover::OnHistoryDeletionDone,
                     base::Unretained(this)));
    }

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history: we have no mechanism to track when these items were
    // created, so we'll clear them all. Better safe than sorry.
    if (g_browser_process->io_thread()) {
      waiting_for_clear_networking_history_ = true;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BrowsingDataRemover::ClearNetworkingHistory,
                     base::Unretained(this), g_browser_process->io_thread()));
    }

    // As part of history deletion we also delete the auto-generated keywords.
    TemplateURLService* keywords_model =
        TemplateURLServiceFactory::GetForProfile(profile_);
    if (keywords_model && !keywords_model->loaded()) {
      registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                     content::Source<TemplateURLService>(keywords_model));
      keywords_model->Load();
    } else if (keywords_model) {
      keywords_model->RemoveAutoGeneratedForOriginBetween(remove_origin_,
          delete_begin_, delete_end_);
    }

    // The PrerenderManager keeps history of prerendered pages, so clear that.
    // It also may have a prerendered page. If so, the page could be
    // considered to have a small amount of historical information, so delete
    // it, too.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS |
          prerender::PrerenderManager::CLEAR_PRERENDER_HISTORY);
    }

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    if (remove_origin_.is_empty()) {
      // We also delete the list of recently closed tabs. Since these expire,
      // they can't be more than a day old, so we can simply clear them all.
      TabRestoreService* tab_service =
          TabRestoreServiceFactory::GetForProfile(profile_);
      if (tab_service) {
        tab_service->ClearEntries();
        tab_service->DeleteLastSession();
      }

#if defined(ENABLE_SESSION_SERVICE)
      // We also delete the last session when we delete the history.
      SessionService* session_service =
          SessionServiceFactory::GetForProfile(profile_);
      if (session_service)
        session_service->DeleteLastSession();
#endif
    }
  }

  if (remove_mask & REMOVE_DOWNLOADS) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(profile_);
    download_manager->RemoveDownloadsBetween(delete_begin_, delete_end_);
    DownloadService* download_service =
        DownloadServiceFactory::GetForProfile(profile_);
    ChromeDownloadManagerDelegate* download_manager_delegate =
        download_service->GetDownloadManagerDelegate();
    download_manager_delegate->ClearLastDownloadPath();
  }

  // We ignore the REMOVE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request REMOVE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & REMOVE_COOKIES &&
      origin_set_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));
    // Since we are running on the UI thread don't call GetURLRequestContext().
    net::URLRequestContextGetter* rq_context = profile_->GetRequestContext();
    if (rq_context) {
      ++waiting_for_clear_cookies_count_;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BrowsingDataRemover::ClearCookiesOnIOThread,
                     base::Unretained(this), base::Unretained(rq_context)));
    }

#if defined(ENABLE_SAFE_BROWSING)
    // Clear the safebrowsing cookies only if time period is for "all time".  It
    // doesn't make sense to apply the time period of deleting in the last X
    // hours/days to the safebrowsing cookies since they aren't the result of
    // any user action.
    if (delete_begin_ == base::Time()) {
      SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      if (sb_service) {
        net::URLRequestContextGetter* sb_context =
            sb_service->url_request_context();
        ++waiting_for_clear_cookies_count_;
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&BrowsingDataRemover::ClearCookiesOnIOThread,
                       base::Unretained(this), base::Unretained(sb_context)));
      }
    }
#endif
  }

  // Server bound certs are not separated for protected and unprotected web
  // origins. We check the origin_set_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_SERVER_BOUND_CERTS &&
      origin_set_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ServerBoundCerts"));
    // Since we are running on the UI thread don't call GetURLRequestContext().
    net::URLRequestContextGetter* rq_context = profile_->GetRequestContext();
    if (rq_context) {
      waiting_for_clear_server_bound_certs_ = true;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BrowsingDataRemover::ClearServerBoundCertsOnIOThread,
                     base::Unretained(this), base::Unretained(rq_context)));
    }
  }

  if (remove_mask & REMOVE_LOCAL_STORAGE) {
    waiting_for_clear_local_storage_ = true;
    if (!dom_storage_context_) {
      dom_storage_context_ =
          BrowserContext::GetDefaultStoragePartition(profile_)->
              GetDOMStorageContext();
    }
    ClearLocalStorageOnUIThread();
  }

  if (remove_mask & REMOVE_INDEXEDDB || remove_mask & REMOVE_WEBSQL ||
      remove_mask & REMOVE_APPCACHE || remove_mask & REMOVE_FILE_SYSTEMS) {
    if (!quota_manager_) {
      quota_manager_ =
          BrowserContext::GetDefaultStoragePartition(profile_)->
              GetQuotaManager();
    }
    waiting_for_clear_quota_managed_data_ = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataRemover::ClearQuotaManagedDataOnIOThread,
                   base::Unretained(this)));
  }

  // Plugin is data not separated for protected and unprotected web origins. We
  // check the origin_set_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_PLUGIN_DATA &&
      origin_set_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_LSOData"));

    waiting_for_clear_plugin_data_ = true;
    if (!plugin_data_remover_.get())
      plugin_data_remover_.reset(content::PluginDataRemover::Create(profile_));
    base::WaitableEvent* event =
        plugin_data_remover_->StartRemoving(delete_begin_);
    watcher_.StartWatching(event, this);
  }

  if (remove_mask & REMOVE_PASSWORDS) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    PasswordStore* password_store = PasswordStoreFactory::GetForProfile(
        profile_, Profile::EXPLICIT_ACCESS);

    if (password_store)
      password_store->RemoveLoginsCreatedBetween(delete_begin_, delete_end_);
  }

  if (remove_mask & REMOVE_FORM_DATA) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<WebDataService> web_data_service =
        WebDataServiceFactory::GetForProfile(profile_,
                                             Profile::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      waiting_for_clear_form_ = true;
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
          delete_end_);
      web_data_service->RemoveAutofillProfilesAndCreditCardsModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTask(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&BrowsingDataRemover::FormDataDBThreadHop,
                     base::Unretained(this)));

      PersonalDataManager* data_manager =
          PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager) {
        data_manager->Refresh();
      }
    }
  }

  if (remove_mask & REMOVE_CACHE) {
    // Tell the renderers to clear their cache.
    WebCacheManager::GetInstance()->ClearCache();

    // Invoke DoClearCache on the IO thread.
    waiting_for_clear_cache_ = true;
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataRemover::ClearCacheOnIOThread,
                   base::Unretained(this)));

#if !defined(DISABLE_NACL)
    waiting_for_clear_nacl_cache_ = true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataRemover::ClearNaClCacheOnIOThread,
                   base::Unretained(this)));
#endif

    // The PrerenderManager may have a page actively being prerendered, which
    // is essentially a preemptively cached page.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS);
    }
  }

  if (remove_mask & REMOVE_CONTENT_LICENSES) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ContentLicenses"));

    waiting_for_clear_content_licenses_ = true;
    if (!pepper_flash_settings_manager_.get()) {
      pepper_flash_settings_manager_.reset(
          new PepperFlashSettingsManager(this, profile_));
    }
    deauthorize_content_licenses_request_id_ =
        pepper_flash_settings_manager_->DeauthorizeContentLicenses();
  }

  // Also delete cached network related data (like TransportSecurityState,
  // HttpServerProperties data).
  profile_->ClearNetworkingHistorySince(delete_begin_);

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemover::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemover::OnHistoryDeletionDone() {
  waiting_for_clear_history_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OverrideQuotaManagerForTesting(
    quota::QuotaManager* quota_manager) {
  quota_manager_ = quota_manager;
}

base::Time BrowsingDataRemover::CalculateBeginDeleteTime(
    TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case LAST_HOUR:
      diff = base::TimeDelta::FromHours(1);
      break;
    case LAST_DAY:
      diff = base::TimeDelta::FromHours(24);
      break;
    case LAST_WEEK:
      diff = base::TimeDelta::FromHours(7*24);
      break;
    case FOUR_WEEKS:
      diff = base::TimeDelta::FromHours(4*7*24);
      break;
    case EVERYTHING:
      delete_begin_time = base::Time();
      break;
    default:
      NOTREACHED() << L"Missing item";
      break;
  }
  return delete_begin_time - diff;
}

bool BrowsingDataRemover::AllDone() {
  return registrar_.IsEmpty() &&
      !waiting_for_clear_cache_ &&
      !waiting_for_clear_nacl_cache_ &&
      !waiting_for_clear_cookies_count_&&
      !waiting_for_clear_history_ &&
      !waiting_for_clear_local_storage_ &&
      !waiting_for_clear_networking_history_ &&
      !waiting_for_clear_server_bound_certs_ &&
      !waiting_for_clear_plugin_data_ &&
      !waiting_for_clear_quota_managed_data_ &&
      !waiting_for_clear_content_licenses_ &&
      !waiting_for_clear_form_;
}

void BrowsingDataRemover::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  // TODO(brettw) bug 1139736: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.
  DCHECK(type == chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED);
  TemplateURLService* model = content::Source<TemplateURLService>(source).ptr();
  if (model->profile() == profile_) {
    registrar_.RemoveAll();
    model->RemoveAutoGeneratedBetween(delete_begin_, delete_end_);
    NotifyAndDeleteIfDone();
  }
}

void BrowsingDataRemover::NotifyAndDeleteIfDone() {
  // TODO(brettw) bug 1139736: see TODO in Observe() above.
  if (!AllDone())
    return;

  set_removing(false);

  // Send global notification, then notify any explicit observers.
  BrowsingDataRemover::NotificationDetails details(delete_begin_, remove_mask_,
      origin_set_mask_);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
      content::Source<Profile>(profile_),
      content::Details<BrowsingDataRemover::NotificationDetails>(&details));

  FOR_EACH_OBSERVER(Observer, observer_list_, OnBrowsingDataRemoverDone());

  // History requests aren't happy if you delete yourself from the callback.
  // As such, we do a delete later.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void BrowsingDataRemover::ClearedNetworkHistory() {
  waiting_for_clear_networking_history_ = false;

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearNetworkingHistory(IOThread* io_thread) {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  io_thread->ClearHostCache();

  chrome_browser_net::Predictor* predictor = profile_->GetNetworkPredictor();
  if (predictor) {
    predictor->DiscardInitialNavigationHistory();
    predictor->DiscardAllResults();
  }

  // Notify the UI thread that we are done.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::ClearedNetworkHistory,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearedCache() {
  waiting_for_clear_cache_ = false;

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearCacheOnIOThread() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(STATE_NONE, next_cache_state_);
  DCHECK(main_context_getter_);
  DCHECK(media_context_getter_);

  next_cache_state_ = STATE_CREATE_MAIN;
  DoClearCache(net::OK);
}

// The expected state sequence is STATE_NONE --> STATE_CREATE_MAIN -->
// STATE_DELETE_MAIN --> STATE_CREATE_MEDIA --> STATE_DELETE_MEDIA -->
// STATE_DONE, and any errors are ignored.
void BrowsingDataRemover::DoClearCache(int rv) {
  DCHECK_NE(STATE_NONE, next_cache_state_);

  while (rv != net::ERR_IO_PENDING && next_cache_state_ != STATE_NONE) {
    switch (next_cache_state_) {
      case STATE_CREATE_MAIN:
      case STATE_CREATE_MEDIA: {
        // Get a pointer to the cache.
        net::URLRequestContextGetter* getter =
            (next_cache_state_ == STATE_CREATE_MAIN) ?
                main_context_getter_ : media_context_getter_;
        net::HttpTransactionFactory* factory =
            getter->GetURLRequestContext()->http_transaction_factory();

        rv = factory->GetCache()->GetBackend(
            &cache_, base::Bind(&BrowsingDataRemover::DoClearCache,
                                base::Unretained(this)));
        next_cache_state_ = (next_cache_state_ == STATE_CREATE_MAIN) ?
                                STATE_DELETE_MAIN : STATE_DELETE_MEDIA;
        break;
      }
      case STATE_DELETE_MAIN:
      case STATE_DELETE_MEDIA: {
        // |cache_| can be null if it cannot be initialized.
        if (cache_) {
          if (delete_begin_.is_null()) {
            rv = cache_->DoomAllEntries(
                base::Bind(&BrowsingDataRemover::DoClearCache,
                           base::Unretained(this)));
          } else {
            rv = cache_->DoomEntriesBetween(
                delete_begin_, delete_end_,
                base::Bind(&BrowsingDataRemover::DoClearCache,
                           base::Unretained(this)));
          }
          cache_ = NULL;
        }
        next_cache_state_ = (next_cache_state_ == STATE_DELETE_MAIN) ?
                                STATE_CREATE_MEDIA : STATE_DONE;
        break;
      }
      case STATE_DONE: {
        cache_ = NULL;

        // Notify the UI thread that we are done.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&BrowsingDataRemover::ClearedCache,
                       base::Unretained(this)));

        next_cache_state_ = STATE_NONE;
        break;
      }
      default: {
        NOTREACHED() << "bad state";
        next_cache_state_ = STATE_NONE;  // Stop looping.
        break;
      }
    }
  }
}

#if !defined(DISABLE_NACL)
void BrowsingDataRemover::ClearedNaClCache() {
  // This function should be called on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  waiting_for_clear_nacl_cache_ = false;

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearedNaClCacheOnIOThread() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Notify the UI thread that we are done.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::ClearedNaClCache,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearNaClCacheOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  NaClBrowser::GetInstance()->ClearValidationCache(
      base::Bind(&BrowsingDataRemover::ClearedNaClCacheOnIOThread,
                 base::Unretained(this)));
}
#endif

void BrowsingDataRemover::ClearLocalStorageOnUIThread() {
  DCHECK(waiting_for_clear_local_storage_);

  dom_storage_context_->GetUsageInfo(
      base::Bind(&BrowsingDataRemover::OnGotLocalStorageUsageInfo,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnGotLocalStorageUsageInfo(
    const std::vector<dom_storage::DomStorageContext::UsageInfo>& infos) {
  DCHECK(waiting_for_clear_local_storage_);

  for (size_t i = 0; i < infos.size(); ++i) {
    if (!BrowsingDataHelper::DoesOriginMatchMask(infos[i].origin,
                                                 origin_set_mask_,
                                                 special_storage_policy_))
      continue;

    if (infos[i].last_modified >= delete_begin_ &&
        infos[i].last_modified <= delete_end_)
      dom_storage_context_->DeleteOrigin(infos[i].origin);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnLocalStorageCleared,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnLocalStorageCleared() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(waiting_for_clear_local_storage_);
  waiting_for_clear_local_storage_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearQuotaManagedDataOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Ask the QuotaManager for all origins with temporary quota modified within
  // the user-specified timeframe, and deal with the resulting set in
  // OnGotQuotaManagedOrigins().
  quota_managed_origins_to_delete_count_ = 0;
  quota_managed_storage_types_to_delete_count_ = 0;

  if (delete_begin_ == base::Time() ||
      origin_set_mask_ &
          (BrowsingDataHelper::PROTECTED_WEB | BrowsingDataHelper::EXTENSION)) {
    // If we're deleting since the beginning of time, or we're removing
    // protected origins, then ask the QuotaManager for all origins with
    // persistent quota modified within the user-specified timeframe, and deal
    // with the resulting set in OnGotQuotaManagedOrigins.
    ++quota_managed_storage_types_to_delete_count_;
    quota_manager_->GetOriginsModifiedSince(
        quota::kStorageTypePersistent, delete_begin_,
        base::Bind(&BrowsingDataRemover::OnGotQuotaManagedOrigins,
                   base::Unretained(this)));
  }

  // Do the same for temporary quota.
  ++quota_managed_storage_types_to_delete_count_;
  quota_manager_->GetOriginsModifiedSince(
      quota::kStorageTypeTemporary, delete_begin_,
      base::Bind(&BrowsingDataRemover::OnGotQuotaManagedOrigins,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnGotQuotaManagedOrigins(
    const std::set<GURL>& origins, quota::StorageType type) {
  DCHECK_GT(quota_managed_storage_types_to_delete_count_, 0);
  // Walk through the origins passed in, delete quota of |type| from each that
  // matches the |origin_set_mask_|.
  std::set<GURL>::const_iterator origin;
  for (origin = origins.begin(); origin != origins.end(); ++origin) {
    // TODO(mkwst): Clean this up, it's slow. http://crbug.com/130746
    if (!remove_origin_.is_empty() && remove_origin_ != origin->GetOrigin())
      continue;

    if (!BrowsingDataHelper::DoesOriginMatchMask(origin->GetOrigin(),
                                                 origin_set_mask_,
                                                 special_storage_policy_))
      continue;

    ++quota_managed_origins_to_delete_count_;
    quota_manager_->DeleteOriginData(
        origin->GetOrigin(), type,
        BrowsingDataRemover::GenerateQuotaClientMask(remove_mask_),
        base::Bind(&BrowsingDataRemover::OnQuotaManagedOriginDeletion,
                   base::Unretained(this), origin->GetOrigin(), type));
  }

  --quota_managed_storage_types_to_delete_count_;
  CheckQuotaManagedDataDeletionStatus();
}

void BrowsingDataRemover::OnQuotaManagedOriginDeletion(
    const GURL& origin,
    quota::StorageType type,
    quota::QuotaStatusCode status) {
  DCHECK_GT(quota_managed_origins_to_delete_count_, 0);
  if (status != quota::kQuotaStatusOk) {
    DLOG(ERROR) << "Couldn't remove data of type " << type << " for origin "
                << origin << ". Status: " << status;
  }

  --quota_managed_origins_to_delete_count_;
  CheckQuotaManagedDataDeletionStatus();
}

void BrowsingDataRemover::CheckQuotaManagedDataDeletionStatus() {
  if (quota_managed_storage_types_to_delete_count_ != 0 ||
      quota_managed_origins_to_delete_count_ != 0) {
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnQuotaManagedDataDeleted,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnQuotaManagedDataDeleted() {
  DCHECK(waiting_for_clear_quota_managed_data_);
  waiting_for_clear_quota_managed_data_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
  waiting_for_clear_plugin_data_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnDeauthorizeContentLicensesCompleted(
    uint32 request_id,
    bool /* success */) {
  DCHECK(waiting_for_clear_content_licenses_);
  DCHECK_EQ(request_id, deauthorize_content_licenses_request_id_);

  waiting_for_clear_content_licenses_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnClearedCookies(int num_deleted) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BrowsingDataRemover::OnClearedCookies,
                   base::Unretained(this), num_deleted));
    return;
  }

  DCHECK(waiting_for_clear_cookies_count_ > 0);
  --waiting_for_clear_cookies_count_;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearCookiesOnIOThread(
    net::URLRequestContextGetter* rq_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* cookie_store = rq_context->
      GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenAsync(
      delete_begin_, delete_end_,
      base::Bind(&BrowsingDataRemover::OnClearedCookies,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearServerBoundCertsOnIOThread(
    net::URLRequestContextGetter* rq_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::ServerBoundCertService* server_bound_cert_service =
      rq_context->GetURLRequestContext()->server_bound_cert_service();
  server_bound_cert_service->GetCertStore()->DeleteAllCreatedBetween(
      delete_begin_, delete_end_);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnClearedServerBoundCerts,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnClearedServerBoundCerts() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  waiting_for_clear_server_bound_certs_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::FormDataDBThreadHop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnClearedFormData,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnClearedFormData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  waiting_for_clear_form_ = false;
  NotifyAndDeleteIfDone();
}
