// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/content/storage_partition_http_cache_data_remover.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/domain_reliability/service.h"
#include "components/history/core/browser/history_service.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/power/origin_power_map.h"
#include "components/power/origin_power_map_factory.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_ui_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_data_remover.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/features/features.h"
#include "media/media_features.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/features/features.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/precache/content/precache_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "chrome/browser/media/webrtc/webrtc_log_list.h"
#include "chrome/browser/media/webrtc/webrtc_log_util.h"
#endif

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace {

void UIThreadTrampolineHelper(const base::Closure& callback) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

// Convenience method to create a callback that can be run on any thread and
// will post the given |callback| back to the UI thread.
base::Closure UIThreadTrampoline(const base::Closure& callback) {
  // We could directly bind &BrowserThread::PostTask, but that would require
  // evaluating FROM_HERE when this method is called, as opposed to when the
  // task is actually posted.
  return base::Bind(&UIThreadTrampolineHelper, callback);
}

template <typename T>
void IgnoreArgumentHelper(const base::Closure& callback, T unused_argument) {
  callback.Run();
}

// Another convenience method to turn a callback without arguments into one that
// accepts (and ignores) a single argument.
template <typename T>
base::Callback<void(T)> IgnoreArgument(const base::Closure& callback) {
  return base::Bind(&IgnoreArgumentHelper<T>, callback);
}

// Helper to create callback for BrowsingDataRemover::DoesOriginMatchMask.
bool DoesOriginMatchMaskAndUrls(
    int origin_type_mask,
    const base::Callback<bool(const GURL&)>& predicate,
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return predicate.Run(origin) &&
         BrowsingDataHelper::DoesOriginMatchMask(origin, origin_type_mask,
                                                 special_storage_policy);
}

bool ForwardPrimaryPatternCallback(
    const base::Callback<bool(const ContentSettingsPattern&)> predicate,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  return predicate.Run(primary_pattern);
}

void ClearHostnameResolutionCacheOnIOThread(
    IOThread* io_thread,
    base::Callback<bool(const std::string&)> host_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  io_thread->ClearHostCache(host_filter);
}

void ClearHttpAuthCacheOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    base::Time delete_begin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::HttpNetworkSession* http_session = context_getter->GetURLRequestContext()
                                              ->http_transaction_factory()
                                              ->GetSession();
  DCHECK(http_session);
  http_session->http_auth_cache()->ClearEntriesAddedWithin(base::Time::Now() -
                                                           delete_begin);
  http_session->CloseAllConnections();
}

void ClearNetworkPredictorOnIOThread(chrome_browser_net::Predictor* predictor) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(predictor);

  predictor->DiscardInitialNavigationHistory();
  predictor->DiscardAllResults();
}

#if !defined(DISABLE_NACL)
void ClearNaClCacheOnIOThread(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  nacl::NaClBrowser::GetInstance()->ClearValidationCache(callback);
}

void ClearPnaclCacheOnIOThread(base::Time begin,
                               base::Time end,
                               const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  pnacl::PnaclHost::GetInstance()->ClearTranslationCacheEntriesBetween(
      begin, end, callback);
}
#endif

void ClearCookiesOnIOThread(base::Time delete_begin,
                            base::Time delete_end,
                            net::URLRequestContextGetter* rq_context,
                            const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      rq_context->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                             IgnoreArgument<int>(callback));
}

void ClearCookiesWithPredicateOnIOThread(
    base::Time delete_begin,
    base::Time delete_end,
    net::CookieStore::CookiePredicate predicate,
    net::URLRequestContextGetter* rq_context,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      rq_context->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenWithPredicateAsync(
      delete_begin, delete_end, predicate, IgnoreArgument<int>(callback));
}

void OnClearedChannelIDsOnIOThread(net::URLRequestContextGetter* rq_context,
                                   const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  rq_context->GetURLRequestContext()
      ->ssl_config_service()
      ->NotifySSLConfigChange();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void ClearChannelIDsOnIOThread(
    const base::Callback<bool(const std::string&)>& domain_predicate,
    base::Time delete_begin,
    base::Time delete_end,
    scoped_refptr<net::URLRequestContextGetter> rq_context,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDService* channel_id_service =
      rq_context->GetURLRequestContext()->channel_id_service();
  channel_id_service->GetChannelIDStore()->DeleteForDomainsCreatedBetween(
      domain_predicate, delete_begin, delete_end,
      base::Bind(&OnClearedChannelIDsOnIOThread,
                 base::RetainedRef(std::move(rq_context)), callback));
}

}  // namespace

BrowsingDataRemover::CompletionInhibitor*
    BrowsingDataRemover::completion_inhibitor_ = nullptr;

bool BrowsingDataRemover::TimeRange::operator==(
    const BrowsingDataRemover::TimeRange& other) const {
  return begin == other.begin && end == other.end;
}

// static
BrowsingDataRemover::TimeRange BrowsingDataRemover::Unbounded() {
  return TimeRange(base::Time(), base::Time::Max());
}

// static
BrowsingDataRemover::TimeRange BrowsingDataRemover::Period(
    browsing_data::TimePeriod period) {
  switch (period) {
    case browsing_data::LAST_HOUR:
      content::RecordAction(UserMetricsAction("ClearBrowsingData_LastHour"));
      break;
    case browsing_data::LAST_DAY:
      content::RecordAction(UserMetricsAction("ClearBrowsingData_LastDay"));
      break;
    case browsing_data::LAST_WEEK:
      content::RecordAction(UserMetricsAction("ClearBrowsingData_LastWeek"));
      break;
    case browsing_data::FOUR_WEEKS:
      content::RecordAction(UserMetricsAction("ClearBrowsingData_LastMonth"));
      break;
    case browsing_data::ALL_TIME:
      content::RecordAction(UserMetricsAction("ClearBrowsingData_Everything"));
      break;
  }
  return TimeRange(CalculateBeginDeleteTime(period), base::Time::Max());
}

BrowsingDataRemover::BrowsingDataRemover(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)),
      remove_mask_(-1),
      origin_type_mask_(-1),
      is_removing_(false),
#if BUILDFLAG(ENABLE_PLUGINS)
      flash_lso_helper_(BrowsingDataFlashLSOHelper::Create(profile_)),
#endif
#if BUILDFLAG(ANDROID_JAVA_UI)
      webapp_registry_(new WebappRegistry()),
#endif
      weak_ptr_factory_(this) {
  DCHECK(browser_context);
}

BrowsingDataRemover::~BrowsingDataRemover() {
  if (!task_queue_.empty()) {
    VLOG(1) << "BrowsingDataRemover shuts down with " << task_queue_.size()
            << " pending tasks";
  }

  // If we are still removing data, notify observers that their task has been
  // (albeit unsucessfuly) processed, so they can unregister themselves.
  // TODO(bauerb): If it becomes a problem that browsing data might not actually
  // be fully cleared when an observer is notified, add a success flag.
  while (!task_queue_.empty()) {
    if (observer_list_.HasObserver(task_queue_.front().observer))
      task_queue_.front().observer->OnBrowsingDataRemoverDone();
    task_queue_.pop();
  }
}

void BrowsingDataRemover::Shutdown() {
  history_task_tracker_.TryCancelAll();
  template_url_sub_.reset();
}

void BrowsingDataRemover::SetRemoving(bool is_removing) {
  DCHECK_NE(is_removing_, is_removing);
  is_removing_ = is_removing;
}

void BrowsingDataRemover::Remove(const TimeRange& time_range,
                                 int remove_mask,
                                 int origin_type_mask) {
  RemoveInternal(time_range, remove_mask, origin_type_mask,
                 std::unique_ptr<RegistrableDomainFilterBuilder>(), nullptr);
}

void BrowsingDataRemover::RemoveAndReply(
    const TimeRange& time_range,
    int remove_mask,
    int origin_type_mask,
    Observer* observer) {
  DCHECK(observer);
  RemoveInternal(time_range, remove_mask, origin_type_mask,
                 std::unique_ptr<RegistrableDomainFilterBuilder>(), observer);
}

void BrowsingDataRemover::RemoveWithFilter(
    const TimeRange& time_range,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
  DCHECK_EQ(0, remove_mask & ~FILTERABLE_DATATYPES);
  DCHECK(filter_builder);
  RemoveInternal(time_range, remove_mask, origin_type_mask,
                 std::move(filter_builder), nullptr);
}

void BrowsingDataRemover::RemoveWithFilterAndReply(
    const TimeRange& time_range,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK_EQ(0, remove_mask & ~FILTERABLE_DATATYPES);
  DCHECK(filter_builder);
  DCHECK(observer);
  RemoveInternal(time_range, remove_mask, origin_type_mask,
                 std::move(filter_builder), observer);
}

void BrowsingDataRemover::RemoveInternal(
    const TimeRange& time_range,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(!observer || observer_list_.HasObserver(observer))
      << "Every observer must register itself (by calling AddObserver()) "
      << "before observing a removal task.";

  // Remove() and RemoveAndReply() pass a null pointer to indicate no filter.
  // No filter is equivalent to one that |IsEmptyBlacklist()|.
  if (!filter_builder) {
    filter_builder = base::MakeUnique<RegistrableDomainFilterBuilder>(
        RegistrableDomainFilterBuilder::BLACKLIST);
    DCHECK(filter_builder->IsEmptyBlacklist());
  }

  task_queue_.emplace(
      time_range,
      remove_mask,
      origin_type_mask,
      std::move(filter_builder),
      observer);

  // If this is the only scheduled task, execute it immediately. Otherwise,
  // it will be automatically executed when all tasks scheduled before it
  // finish.
  if (task_queue_.size() == 1) {
    SetRemoving(true);
    RunNextTask();
  }
}

void BrowsingDataRemover::RunNextTask() {
  DCHECK(!task_queue_.empty());
  const RemovalTask& removal_task = task_queue_.front();

  RemoveImpl(removal_task.time_range,
             removal_task.remove_mask,
             *removal_task.filter_builder,
             removal_task.origin_type_mask);
}

void BrowsingDataRemover::RemoveImpl(
    const TimeRange& time_range,
    int remove_mask,
    const BrowsingDataFilterBuilder& filter_builder,
    int origin_type_mask) {
  // =============== README before adding more storage backends ===============
  //
  // If you're adding a data storage backend that is included among
  // RemoveDataMask::FILTERABLE_DATATYPES, you must do one of the following:
  // 1. Support one of the filters generated by |filter_builder|.
  // 2. Add a comment explaining why is it acceptable in your case to delete all
  //    data without filtering URLs / origins / domains.
  // 3. Do not support partial deletion, i.e. only delete your data if
  //    |filter_builder.IsEmptyBlacklist()|. Add a comment explaining why this
  //    is acceptable.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_synchronous_clear_operations_ = true;

  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max().
  DCHECK_NE(base::Time(), time_range.end);

  delete_begin_ = time_range.begin;
  delete_end_ = time_range.end;
  remove_mask_ = remove_mask;
  origin_type_mask_ = origin_type_mask;

  base::Callback<bool(const GURL& url)> filter =
      filter_builder.BuildGeneralFilter();
  base::Callback<bool(const ContentSettingsPattern& url)> same_pattern_filter =
      filter_builder.BuildWebsiteSettingsPatternMatchesFilter();

  // Some backends support a filter that |is_null()| to make complete deletion
  // more efficient.
  base::Callback<bool(const GURL&)> nullable_filter =
      filter_builder.IsEmptyBlacklist() ? base::Callback<bool(const GURL&)>()
                                        : filter;

  PrefService* prefs = profile_->GetPrefs();
  bool may_delete_history = prefs->GetBoolean(
      prefs::kAllowDeletingBrowserHistory);

  // All the UI entry points into the BrowsingDataRemover should be disabled,
  // but this will fire if something was missed or added.
  DCHECK(may_delete_history || (remove_mask & REMOVE_NOCHECKS) ||
      (!(remove_mask & REMOVE_HISTORY) && !(remove_mask & REMOVE_DOWNLOADS)));

  if (origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));
  }
  if (origin_type_mask_ & BrowsingDataHelper::PROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsProtectedWeb"));
  }
  if (origin_type_mask_ & BrowsingDataHelper::EXTENSION) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsExtension"));
  }
  // If this fires, we added a new BrowsingDataHelper::OriginTypeMask without
  // updating the user metrics above.
  static_assert(
      BrowsingDataHelper::ALL == (BrowsingDataHelper::UNPROTECTED_WEB |
                                  BrowsingDataHelper::PROTECTED_WEB |
                                  BrowsingDataHelper::EXTENSION),
      "OriginTypeMask has been updated without updating user metrics");

  if ((remove_mask & REMOVE_HISTORY) && may_delete_history) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      content::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      waiting_for_clear_history_ = true;
      history_service->ExpireLocalAndRemoteHistoryBetween(
          WebHistoryServiceFactory::GetForProfile(profile_), std::set<GURL>(),
          delete_begin_, delete_end_,
          base::Bind(&BrowsingDataRemover::OnHistoryDeletionDone,
                     weak_ptr_factory_.GetWeakPtr()),
          &history_task_tracker_);
    }

    ntp_snippets::ContentSuggestionsService* content_suggestions_service =
        ContentSuggestionsServiceFactory::GetForProfileIfExists(profile_);
    if (content_suggestions_service) {
      content_suggestions_service->ClearHistory(delete_begin_, delete_end_,
                                                filter);
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // The extension activity log contains details of which websites extensions
    // were active on. It therefore indirectly stores details of websites a
    // user has visited so best clean from here as well.
    // TODO(msramek): Support all backends with filter (crbug.com/589586).
    extensions::ActivityLog::GetInstance(profile_)->RemoveURLs(
        std::set<GURL>());

    // Clear launch times as they are a form of history.
    // BrowsingDataFilterBuilder currently doesn't support extension origins.
    // Therefore, clearing history for a small set of origins (WHITELIST) should
    // never delete any extension launch times, while clearing for almost all
    // origins (BLACKLIST) should always delete all of extension launch times.
    if (filter_builder.mode() == BrowsingDataFilterBuilder::BLACKLIST) {
      extensions::ExtensionPrefs* extension_prefs =
          extensions::ExtensionPrefs::Get(profile_);
      extension_prefs->ClearLastLaunchTimes();
    }
#endif

    // The power consumption history by origin contains details of websites
    // that were visited.
    power::OriginPowerMap* origin_power_map =
        power::OriginPowerMapFactory::GetForBrowserContext(profile_);
    if (origin_power_map)
      origin_power_map->ClearOriginMap(nullable_filter);

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history. We have no mechanism to track when these items were
    // created, so we'll not honor the time range.
    // TODO(msramek): We can use the plugin filter here because plugins, same
    // as the hostname resolution cache, key their entries by hostname. Rename
    // BuildPluginFilter() to something more general to reflect this use.
    if (g_browser_process->io_thread()) {
      waiting_for_clear_hostname_resolution_cache_ = true;
      BrowserThread::PostTaskAndReply(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ClearHostnameResolutionCacheOnIOThread,
                     g_browser_process->io_thread(),
                     filter_builder.BuildPluginFilter()),
          base::Bind(&BrowsingDataRemover::OnClearedHostnameResolutionCache,
                     weak_ptr_factory_.GetWeakPtr()));
    }
    if (profile_->GetNetworkPredictor()) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      waiting_for_clear_network_predictor_ = true;
      BrowserThread::PostTaskAndReply(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ClearNetworkPredictorOnIOThread,
                     profile_->GetNetworkPredictor()),
          base::Bind(&BrowsingDataRemover::OnClearedNetworkPredictor,
                     weak_ptr_factory_.GetWeakPtr()));
      profile_->GetNetworkPredictor()->ClearPrefsOnUIThread();
    }

    // As part of history deletion we also delete the auto-generated keywords.
    TemplateURLService* keywords_model =
        TemplateURLServiceFactory::GetForProfile(profile_);

    if (keywords_model && !keywords_model->loaded()) {
      // TODO(msramek): Store filters from the currently executed task on the
      // object to avoid having to copy them to callback methods.
      template_url_sub_ = keywords_model->RegisterOnLoadedCallback(
          base::Bind(&BrowsingDataRemover::OnKeywordsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), filter));
      keywords_model->Load();
      waiting_for_clear_keyword_data_ = true;
    } else if (keywords_model) {
      keywords_model->RemoveAutoGeneratedForUrlsBetween(filter, delete_begin_,
                                                        delete_end_);
    }

    // The PrerenderManager keeps history of prerendered pages, so clear that.
    // It also may have a prerendered page. If so, the page could be
    // considered to have a small amount of historical information, so delete
    // it, too.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
    if (prerender_manager) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS |
          prerender::PrerenderManager::CLEAR_PRERENDER_HISTORY);
    }

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    if (filter_builder.IsEmptyBlacklist()) {
      // We also delete the list of recently closed tabs. Since these expire,
      // they can't be more than a day old, so we can simply clear them all.
      sessions::TabRestoreService* tab_service =
          TabRestoreServiceFactory::GetForProfile(profile_);
      if (tab_service) {
        tab_service->ClearEntries();
        tab_service->DeleteLastSession();
      }

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
      // We also delete the last session when we delete the history.
      SessionService* session_service =
          SessionServiceFactory::GetForProfile(profile_);
      if (session_service)
        session_service->DeleteLastSession();
#endif
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    // TODO(dmurph): Support all backends with filter (crbug.com/113621).
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      waiting_for_clear_autofill_origin_urls_ = true;
      web_data_service->RemoveOriginURLsModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&BrowsingDataRemover::OnClearedAutofillOriginURLs,
                     weak_ptr_factory_.GetWeakPtr()));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }

#if BUILDFLAG(ENABLE_WEBRTC)
    waiting_for_clear_webrtc_logs_ = true;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &WebRtcLogUtil::DeleteOldAndRecentWebRtcLogFiles,
            WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile_->GetPath()),
            delete_begin_),
        base::Bind(&BrowsingDataRemover::OnClearedWebRtcLogs,
                   weak_ptr_factory_.GetWeakPtr()));
#endif

    // The SSL Host State that tracks SSL interstitial "proceed" decisions may
    // include origins that the user has visited, so it must be cleared.
    // TODO(msramek): We can reuse the plugin filter here, since both plugins
    // and SSL host state are scoped to hosts and represent them as std::string.
    // Rename the method to indicate its more general usage.
    if (profile_->GetSSLHostStateDelegate()) {
      profile_->GetSSLHostStateDelegate()->Clear(
          filter_builder.IsEmptyBlacklist()
              ? base::Callback<bool(const std::string&)>()
              : filter_builder.BuildPluginFilter());
    }

#if BUILDFLAG(ANDROID_JAVA_UI)
    precache::PrecacheManager* precache_manager =
        precache::PrecacheManagerFactory::GetForBrowserContext(profile_);
    // |precache_manager| could be nullptr if the profile is off the record.
    if (!precache_manager) {
      waiting_for_clear_precache_history_ = true;
      precache_manager->ClearHistory();
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&BrowsingDataRemover::OnClearedPrecacheHistory,
                     weak_ptr_factory_.GetWeakPtr()));
    }

    // Clear the history information (last launch time and origin URL) of any
    // registered webapps.
    webapp_registry_->ClearWebappHistoryForUrls(filter);
#endif

    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                profile_);
    // |data_reduction_proxy_settings| is null if |profile_| is off the record.
    if (data_reduction_proxy_settings) {
      data_reduction_proxy::DataReductionProxyService*
          data_reduction_proxy_service =
              data_reduction_proxy_settings->data_reduction_proxy_service();
      if (data_reduction_proxy_service) {
        data_reduction_proxy_service->compression_stats()
            ->DeleteBrowsingHistory(delete_begin_, delete_end_);
      }
    }

    // |previews_service| is null if |profile_| is off the record.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(profile_);
    if (previews_service && previews_service->previews_ui_service()) {
      previews_service->previews_ui_service()->ClearBlackList(delete_begin_,
                                                              delete_end_);
    }
  }

  if ((remove_mask & REMOVE_DOWNLOADS) && may_delete_history) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    content::DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(profile_);
    download_manager->RemoveDownloadsByURLAndTime(filter,
                                                  delete_begin_, delete_end_);
    DownloadPrefs* download_prefs = DownloadPrefs::FromDownloadManager(
        download_manager);
    download_prefs->SetSaveFilePath(download_prefs->DownloadPath());
  }

  uint32_t storage_partition_remove_mask = 0;

  // We ignore the REMOVE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request REMOVE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & REMOVE_COOKIES &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));

    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_COOKIES;

    // Clear the safebrowsing cookies only if time period is for "all time".  It
    // doesn't make sense to apply the time period of deleting in the last X
    // hours/days to the safebrowsing cookies since they aren't the result of
    // any user action.
    if (delete_begin_ == base::Time()) {
      safe_browsing::SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      if (sb_service) {
        scoped_refptr<net::URLRequestContextGetter> sb_context =
            sb_service->url_request_context();
        ++waiting_for_clear_cookies_count_;
        if (filter_builder.IsEmptyBlacklist()) {
          BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              base::Bind(&ClearCookiesOnIOThread, delete_begin_, delete_end_,
                         base::RetainedRef(std::move(sb_context)),
                         UIThreadTrampoline(
                             base::Bind(&BrowsingDataRemover::OnClearedCookies,
                                        weak_ptr_factory_.GetWeakPtr()))));
        } else {
          BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              base::Bind(&ClearCookiesWithPredicateOnIOThread, delete_begin_,
                         delete_end_, filter_builder.BuildCookieFilter(),
                         base::RetainedRef(std::move(sb_context)),
                         UIThreadTrampoline(
                             base::Bind(&BrowsingDataRemover::OnClearedCookies,
                                        weak_ptr_factory_.GetWeakPtr()))));
        }
      }
    }

    MediaDeviceIDSalt::Reset(profile_->GetPrefs());
  }

  // Channel IDs are not separated for protected and unprotected web
  // origins. We check the origin_type_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_CHANNEL_IDS &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ChannelIDs"));
    // Since we are running on the UI thread don't call GetURLRequestContext().
    scoped_refptr<net::URLRequestContextGetter> rq_context =
        content::BrowserContext::GetDefaultStoragePartition(profile_)->
          GetURLRequestContext();
    waiting_for_clear_channel_ids_ = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearChannelIDsOnIOThread,
                   filter_builder.BuildChannelIDFilter(),
                   delete_begin_, delete_end_, std::move(rq_context),
                   base::Bind(&BrowsingDataRemover::OnClearedChannelIDs,
                              weak_ptr_factory_.GetWeakPtr())));
  }

  if (remove_mask & REMOVE_DURABLE_PERMISSION) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneTypeWithPredicate(
            CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
            base::Bind(&ForwardPrimaryPatternCallback, same_pattern_filter));
  }

  if (remove_mask & REMOVE_LOCAL_STORAGE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }

  if (remove_mask & REMOVE_INDEXEDDB) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (remove_mask & REMOVE_WEBSQL) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  }
  if (remove_mask & REMOVE_APPCACHE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  }
  if (remove_mask & REMOVE_SERVICE_WORKERS) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (remove_mask & REMOVE_CACHE_STORAGE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  }
  if (remove_mask & REMOVE_FILE_SYSTEMS) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  // Plugin is data not separated for protected and unprotected web origins. We
  // check the origin_type_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_PLUGIN_DATA &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_LSOData"));
    waiting_for_clear_plugin_data_count_ = 1;

    if (filter_builder.IsEmptyBlacklist()) {
      DCHECK(!plugin_data_remover_);
      plugin_data_remover_.reset(content::PluginDataRemover::Create(profile_));
      base::WaitableEvent* event =
          plugin_data_remover_->StartRemoving(delete_begin_);

      base::WaitableEventWatcher::EventCallback watcher_callback =
          base::Bind(&BrowsingDataRemover::OnWaitableEventSignaled,
                     weak_ptr_factory_.GetWeakPtr());
      watcher_.StartWatching(event, watcher_callback);
    } else {
      // TODO(msramek): Store filters from the currently executed task on the
      // object to avoid having to copy them to callback methods.
      flash_lso_helper_->StartFetching(base::Bind(
          &BrowsingDataRemover::OnSitesWithFlashDataFetched,
          weak_ptr_factory_.GetWeakPtr(),
          filter_builder.BuildPluginFilter()));
    }
  }
#endif

  if (remove_mask & REMOVE_SITE_USAGE_DATA) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneTypeWithPredicate(
            CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
            base::Bind(&ForwardPrimaryPatternCallback, same_pattern_filter));
  }

  if (remove_mask & REMOVE_SITE_USAGE_DATA || remove_mask & REMOVE_HISTORY) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneTypeWithPredicate(
            CONTENT_SETTINGS_TYPE_APP_BANNER,
            base::Bind(&ForwardPrimaryPatternCallback, same_pattern_filter));

    PermissionDecisionAutoBlocker::RemoveCountsByUrl(profile_, filter);
  }

  if (remove_mask & REMOVE_PASSWORDS) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      waiting_for_clear_passwords_ = true;
      auto on_cleared_passwords =
          base::Bind(&BrowsingDataRemover::OnClearedPasswords,
                     weak_ptr_factory_.GetWeakPtr());
      password_store->RemoveLoginsByURLAndTime(
          filter, delete_begin_, delete_end_, on_cleared_passwords);
    }
  }

  if (remove_mask & REMOVE_COOKIES) {
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store) {
      waiting_for_clear_auto_sign_in_ = true;
      base::Closure on_cleared_auto_sign_in =
          base::Bind(&BrowsingDataRemover::OnClearedAutoSignIn,
                     weak_ptr_factory_.GetWeakPtr());
      password_store->DisableAutoSignInForOrigins(
          filter, on_cleared_auto_sign_in);
    }
  }

  if (remove_mask & REMOVE_HISTORY) {
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      waiting_for_clear_passwords_stats_ = true;
      password_store->RemoveStatisticsByOriginAndTime(
          nullable_filter, delete_begin_, delete_end_,
          base::Bind(&BrowsingDataRemover::OnClearedPasswordsStats,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // TODO(dmurph): Support all backends with filter (crbug.com/113621).
  if (remove_mask & REMOVE_FORM_DATA) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      waiting_for_clear_form_ = true;
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
          delete_end_);
      web_data_service->RemoveAutofillDataModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&BrowsingDataRemover::OnClearedFormData,
                     weak_ptr_factory_.GetWeakPtr()));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }
  }

  if (remove_mask & REMOVE_CACHE) {
    // Tell the renderers to clear their cache.
    web_cache::WebCacheManager::GetInstance()->ClearCache();

    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    waiting_for_clear_cache_ = true;
    // StoragePartitionHttpCacheDataRemover deletes itself when it is done.
    if (filter_builder.IsEmptyBlacklist()) {
      browsing_data::StoragePartitionHttpCacheDataRemover::CreateForRange(
          BrowserContext::GetDefaultStoragePartition(profile_),
          delete_begin_, delete_end_)
          ->Remove(base::Bind(&BrowsingDataRemover::ClearedCache,
                              weak_ptr_factory_.GetWeakPtr()));
    } else {
      browsing_data::StoragePartitionHttpCacheDataRemover::
          CreateForURLsAndRange(
              BrowserContext::GetDefaultStoragePartition(profile_),
              filter, delete_begin_, delete_end_)
              ->Remove(base::Bind(&BrowsingDataRemover::ClearedCache,
                                  weak_ptr_factory_.GetWeakPtr()));
    }

#if !defined(DISABLE_NACL)
    waiting_for_clear_nacl_cache_ = true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearNaClCacheOnIOThread,
                   UIThreadTrampoline(
                       base::Bind(&BrowsingDataRemover::ClearedNaClCache,
                                  weak_ptr_factory_.GetWeakPtr()))));

    waiting_for_clear_pnacl_cache_ = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearPnaclCacheOnIOThread, delete_begin_, delete_end_,
                   UIThreadTrampoline(
                       base::Bind(&BrowsingDataRemover::ClearedPnaclCache,
                                  weak_ptr_factory_.GetWeakPtr()))));
#endif

    // The PrerenderManager may have a page actively being prerendered, which
    // is essentially a preemptively cached page.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS);
    }

    // Tell the shader disk cache to clear.
    content::RecordAction(UserMetricsAction("ClearBrowsingData_ShaderCache"));
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;

    // When clearing cache, wipe accumulated network related data
    // (TransportSecurityState and HttpServerPropertiesManager data).
    waiting_for_clear_networking_history_ = true;
    profile_->ClearNetworkingHistorySince(
        delete_begin_,
        base::Bind(&BrowsingDataRemover::OnClearedNetworkingHistory,
                   weak_ptr_factory_.GetWeakPtr()));

    ntp_snippets::ContentSuggestionsService* content_suggestions_service =
        ContentSuggestionsServiceFactory::GetForProfileIfExists(profile_);
    if (content_suggestions_service)
      content_suggestions_service->ClearAllCachedSuggestions();

    // |ui_nqe_service| may be null if |profile_| is not a regular profile.
    UINetworkQualityEstimatorService* ui_nqe_service =
        UINetworkQualityEstimatorServiceFactory::GetForProfile(profile_);
    DCHECK(profile_->GetProfileType() !=
               Profile::ProfileType::REGULAR_PROFILE ||
           ui_nqe_service != nullptr);
    if (ui_nqe_service) {
      // Network Quality Estimator (NQE) stores the quality (RTT, bandwidth
      // etc.) of different networks in prefs. The stored quality is not
      // broken down by URLs or timestamps, so clearing the cache should
      // completely clear the prefs.
      ui_nqe_service->ClearPrefs();
    }
  }

  if (remove_mask & REMOVE_COOKIES || remove_mask & REMOVE_PASSWORDS) {
    scoped_refptr<net::URLRequestContextGetter> request_context =
        profile_->GetRequestContext();
    waiting_for_clear_http_auth_cache_ = true;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearHttpAuthCacheOnIOThread, std::move(request_context),
                   delete_begin_),
        base::Bind(&BrowsingDataRemover::OnClearedHttpAuthCache,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Content Decryption Modules used by Encrypted Media store licenses in a
  // private filesystem. These are different than content licenses used by
  // Flash (which are deleted father down in this method).
  if (remove_mask & REMOVE_MEDIA_LICENSES) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA;
  }

  if (storage_partition_remove_mask) {
    waiting_for_clear_storage_partition_data_ = true;

    content::StoragePartition* storage_partition;
    if (storage_partition_for_testing_)
      storage_partition = storage_partition_for_testing_;
    else
      storage_partition = BrowserContext::GetDefaultStoragePartition(profile_);

    uint32_t quota_storage_remove_mask =
        ~content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;

    if (delete_begin_ == base::Time() ||
        origin_type_mask_ &
          (BrowsingDataHelper::PROTECTED_WEB | BrowsingDataHelper::EXTENSION)) {
      // If we're deleting since the beginning of time, or we're removing
      // protected origins, then remove persistent quota data.
      quota_storage_remove_mask |=
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
    }

    // If cookies are supposed to be conditionally deleted from the storage
    // partition, create a cookie matcher function.
    content::StoragePartition::CookieMatcherFunction cookie_matcher;
    if (!filter_builder.IsEmptyBlacklist() &&
        (storage_partition_remove_mask &
            content::StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
      cookie_matcher = filter_builder.BuildCookieFilter();
    }

    storage_partition->ClearData(
        storage_partition_remove_mask, quota_storage_remove_mask,
        base::Bind(&DoesOriginMatchMaskAndUrls, origin_type_mask_, filter),
        cookie_matcher, delete_begin_, delete_end_,
        base::Bind(&BrowsingDataRemover::OnClearedStoragePartitionData,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (remove_mask & REMOVE_MEDIA_LICENSES) {
    // TODO(jrummell): This UMA should be renamed to indicate it is for Media
    // Licenses.
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ContentLicenses"));

#if BUILDFLAG(ENABLE_PLUGINS)
    waiting_for_clear_flash_content_licenses_ = true;
    if (!pepper_flash_settings_manager_.get()) {
      pepper_flash_settings_manager_.reset(
          new PepperFlashSettingsManager(this, profile_));
    }
    deauthorize_flash_content_licenses_request_id_ =
        pepper_flash_settings_manager_->DeauthorizeContentLicenses(prefs);
#if defined(OS_CHROMEOS)
    // On Chrome OS, also delete any content protection platform keys.
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (!user) {
      LOG(WARNING) << "Failed to find user for current profile.";
    } else {
      chromeos::DBusThreadManager::Get()
          ->GetCryptohomeClient()
          ->TpmAttestationDeleteKeys(
              chromeos::attestation::KEY_USER,
              cryptohome::Identification(user->GetAccountId()),
              chromeos::attestation::kContentProtectionKeyPrefix,
              base::Bind(&BrowsingDataRemover::OnClearPlatformKeys,
                         weak_ptr_factory_.GetWeakPtr()));
      waiting_for_clear_platform_keys_ = true;
    }
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  }

  // Remove omnibox zero-suggest cache results. Filtering is not supported.
  // This is not a problem, as deleting more data than necessary will just cause
  // another server round-trip; no data is actually lost.
  if ((remove_mask & (REMOVE_CACHE | REMOVE_COOKIES)))
    prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());

  if (remove_mask & (REMOVE_COOKIES | REMOVE_HISTORY)) {
    domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::
          GetForBrowserContext(profile_);
    if (service) {
      domain_reliability::DomainReliabilityClearMode mode;
      if (remove_mask & REMOVE_COOKIES)
        mode = domain_reliability::CLEAR_CONTEXTS;
      else
        mode = domain_reliability::CLEAR_BEACONS;

      waiting_for_clear_domain_reliability_monitor_ = true;
      service->ClearBrowsingData(
          mode,
          filter,
          base::Bind(&BrowsingDataRemover::OnClearedDomainReliabilityMonitor,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

#if BUILDFLAG(ANDROID_JAVA_UI)
  // Clear all data associated with registered webapps.
  if (remove_mask & REMOVE_WEBAPP_DATA)
    webapp_registry_->UnregisterWebappsForUrls(filter);

  // For now we're considering offline pages as cache, so if we're removing
  // cache we should remove offline pages as well.
  if ((remove_mask & REMOVE_CACHE)) {
    waiting_for_clear_offline_page_data_ = true;
    offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile_)
        ->DeleteCachedPagesByURLPredicate(
            filter, base::Bind(&BrowsingDataRemover::OnClearedOfflinePageData,
                               weak_ptr_factory_.GetWeakPtr()));
  }
#endif

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice = NEITHER_COOKIES_NOR_CACHE;
  if (remove_mask & REMOVE_COOKIES &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    choice = remove_mask & REMOVE_CACHE ? BOTH_COOKIES_AND_CACHE
                                        : ONLY_COOKIES;
  } else if (remove_mask & REMOVE_CACHE) {
    choice = ONLY_CACHE;
  }

  // Notify in case all actions taken were synchronous.
  waiting_for_synchronous_clear_operations_ = false;
  NotifyIfDone();

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache",
      choice, MAX_CHOICE_VALUE);
}

void BrowsingDataRemover::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemover::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemover::OverrideStoragePartitionForTesting(
    content::StoragePartition* storage_partition) {
  storage_partition_for_testing_ = storage_partition;
}

#if BUILDFLAG(ANDROID_JAVA_UI)
void BrowsingDataRemover::OverrideWebappRegistryForTesting(
    std::unique_ptr<WebappRegistry> webapp_registry) {
  webapp_registry_ = std::move(webapp_registry);
}
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
void BrowsingDataRemover::OverrideFlashLSOHelperForTesting(
    scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper) {
  flash_lso_helper_ = flash_lso_helper;
}
#endif

const base::Time& BrowsingDataRemover::GetLastUsedBeginTime() {
  return delete_begin_;
}

const base::Time& BrowsingDataRemover::GetLastUsedEndTime() {
  return delete_end_;
}

int BrowsingDataRemover::GetLastUsedRemovalMask() {
  return remove_mask_;
}

int BrowsingDataRemover::GetLastUsedOriginTypeMask() {
  return origin_type_mask_;
}

BrowsingDataRemover::RemovalTask::RemovalTask(
    const TimeRange& time_range,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer)
    : time_range(time_range),
      remove_mask(remove_mask),
      origin_type_mask(origin_type_mask),
      filter_builder(std::move(filter_builder)),
      observer(observer) {}

BrowsingDataRemover::RemovalTask::~RemovalTask() {}

bool BrowsingDataRemover::AllDone() {
  return !waiting_for_synchronous_clear_operations_ &&
         !waiting_for_clear_autofill_origin_urls_ &&
         !waiting_for_clear_cache_ &&
         !waiting_for_clear_flash_content_licenses_ &&
         !waiting_for_clear_channel_ids_ && !waiting_for_clear_cookies_count_ &&
         !waiting_for_clear_domain_reliability_monitor_ &&
         !waiting_for_clear_form_ && !waiting_for_clear_history_ &&
         !waiting_for_clear_hostname_resolution_cache_ &&
         !waiting_for_clear_http_auth_cache_ &&
         !waiting_for_clear_keyword_data_ && !waiting_for_clear_nacl_cache_ &&
         !waiting_for_clear_network_predictor_ &&
         !waiting_for_clear_networking_history_ &&
         !waiting_for_clear_passwords_ && !waiting_for_clear_passwords_stats_ &&
         !waiting_for_clear_platform_keys_ &&
         !waiting_for_clear_plugin_data_count_ &&
         !waiting_for_clear_pnacl_cache_ &&
#if BUILDFLAG(ANDROID_JAVA_UI)
         !waiting_for_clear_precache_history_ &&
         !waiting_for_clear_offline_page_data_ &&
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
         !waiting_for_clear_webrtc_logs_ &&
#endif
         !waiting_for_clear_storage_partition_data_ &&
         !waiting_for_clear_auto_sign_in_;
}

void BrowsingDataRemover::OnKeywordsLoaded(
    base::Callback<bool(const GURL&)> url_filter) {
  // Deletes the entries from the model, and if we're not waiting on anything
  // else notifies observers and deletes this BrowsingDataRemover.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  model->RemoveAutoGeneratedForUrlsBetween(url_filter, delete_begin_,
                                           delete_end_);
  waiting_for_clear_keyword_data_ = false;
  template_url_sub_.reset();
  NotifyIfDone();
}

void BrowsingDataRemover::Notify() {
  // Some tests call |RemoveImpl| directly, without using the task scheduler.
  // TODO(msramek): Improve those tests so we don't have to do this. Tests
  // relying on |RemoveImpl| do so because they need to pass in
  // BrowsingDataFilterBuilder while still keeping ownership of it. Making
  // BrowsingDataFilterBuilder copyable would solve this.
  if (!is_removing_) {
    DCHECK(task_queue_.empty());
    return;
  }

  // Inform the observer of the current task unless it has unregistered
  // itself in the meantime.
  DCHECK(!task_queue_.empty());

  if (task_queue_.front().observer != nullptr &&
      observer_list_.HasObserver(task_queue_.front().observer)) {
    task_queue_.front().observer->OnBrowsingDataRemoverDone();
  }

  task_queue_.pop();

  if (task_queue_.empty()) {
    // All removal tasks have finished. Inform the observers that we're idle.
    SetRemoving(false);
    return;
  }

  // Yield to the UI thread before executing the next removal task.
  // TODO(msramek): Consider also adding a backoff if too many tasks
  // are scheduled.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::RunNextTask,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowsingDataRemover::NotifyIfDone() {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.

  if (!AllDone())
    return;

  if (completion_inhibitor_) {
    completion_inhibitor_->OnBrowsingDataRemoverWouldComplete(
        this, base::Bind(&BrowsingDataRemover::Notify,
                         weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  Notify();
}

void BrowsingDataRemover::OnHistoryDeletionDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_history_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedHostnameResolutionCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_hostname_resolution_cache_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedHttpAuthCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_http_auth_cache_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedNetworkPredictor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_network_predictor_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedNetworkingHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_networking_history_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::ClearedCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_cache_ = false;
  NotifyIfDone();
}

#if !defined(DISABLE_NACL)
void BrowsingDataRemover::ClearedNaClCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_nacl_cache_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::ClearedPnaclCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_pnacl_cache_ = false;
  NotifyIfDone();
}
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
void BrowsingDataRemover::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(1, waiting_for_clear_plugin_data_count_);
  waiting_for_clear_plugin_data_count_ = 0;

  plugin_data_remover_.reset();
  watcher_.StopWatching();
  NotifyIfDone();
}

void BrowsingDataRemover::OnSitesWithFlashDataFetched(
    base::Callback<bool(const std::string&)> plugin_filter,
    const std::vector<std::string>& sites) {
  DCHECK_EQ(1, waiting_for_clear_plugin_data_count_);
  waiting_for_clear_plugin_data_count_ = 0;

  std::vector<std::string> sites_to_delete;
  for (const std::string& site : sites) {
    if (plugin_filter.Run(site))
      sites_to_delete.push_back(site);
  }

  waiting_for_clear_plugin_data_count_ = sites_to_delete.size();

  for (const std::string& site : sites_to_delete) {
    flash_lso_helper_->DeleteFlashLSOsForSite(
        site,
        base::Bind(&BrowsingDataRemover::OnFlashDataDeleted,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  NotifyIfDone();
}

void BrowsingDataRemover::OnFlashDataDeleted() {
  waiting_for_clear_plugin_data_count_--;
  NotifyIfDone();
}

void BrowsingDataRemover::OnDeauthorizeFlashContentLicensesCompleted(
    uint32_t request_id,
    bool /* success */) {
  DCHECK(waiting_for_clear_flash_content_licenses_);
  DCHECK_EQ(request_id, deauthorize_flash_content_licenses_request_id_);

  waiting_for_clear_flash_content_licenses_ = false;
  NotifyIfDone();
}
#endif

#if defined(OS_CHROMEOS)
void BrowsingDataRemover::OnClearPlatformKeys(
    chromeos::DBusMethodCallStatus call_status,
    bool result) {
  DCHECK(waiting_for_clear_platform_keys_);
  LOG_IF(ERROR, call_status != chromeos::DBUS_METHOD_CALL_SUCCESS || !result)
      << "Failed to clear platform keys.";
  waiting_for_clear_platform_keys_ = false;
  NotifyIfDone();
}
#endif


void BrowsingDataRemover::OnClearedPasswords() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_passwords_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedPasswordsStats() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_passwords_stats_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedAutoSignIn() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_auto_sign_in_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_GT(waiting_for_clear_cookies_count_, 0);
  --waiting_for_clear_cookies_count_;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedChannelIDs() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_channel_ids_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedFormData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_form_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedAutofillOriginURLs() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_autofill_origin_urls_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedStoragePartitionData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_storage_partition_data_ = false;
  NotifyIfDone();
}

#if BUILDFLAG(ENABLE_WEBRTC)
void BrowsingDataRemover::OnClearedWebRtcLogs() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_webrtc_logs_ = false;
  NotifyIfDone();
}
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
void BrowsingDataRemover::OnClearedPrecacheHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_precache_history_ = false;
  NotifyIfDone();
}

void BrowsingDataRemover::OnClearedOfflinePageData(
    offline_pages::OfflinePageModel::DeletePageResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_offline_page_data_ = false;
  NotifyIfDone();
}
#endif

void BrowsingDataRemover::OnClearedDomainReliabilityMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_domain_reliability_monitor_ = false;
  NotifyIfDone();
}
