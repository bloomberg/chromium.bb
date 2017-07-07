// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/content/precache_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/precache/core/precache_database.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "components/precache/core/proto/unfinished_work.pb.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/variations/metrics_util.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace precache {

const char kPrecacheFieldTrialName[] = "Precache";
const char kMinCacheSizeParam[] = "min_cache_size";

namespace {

const char kPrecacheFieldTrialEnabledGroup[] = "Enabled";
const char kPrecacheFieldTrialControlGroup[] = "Control";
const char kConfigURLParam[] = "config_url";
const char kManifestURLPrefixParam[] = "manifest_url_prefix";
const char kDataReductionProxyParam[] = "disable_if_data_reduction_proxy";
const size_t kNumTopHosts = 100;

}  // namespace

size_t NumTopHosts() {
  return kNumTopHosts;
}

PrecacheManager::PrecacheManager(
    content::BrowserContext* browser_context,
    const syncer::SyncService* const sync_service,
    const history::HistoryService* const history_service,
    const data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings,
    const base::FilePath& db_path,
    std::unique_ptr<PrecacheDatabase> precache_database)
    : browser_context_(browser_context),
      sync_service_(sync_service),
      history_service_(history_service),
      data_reduction_proxy_settings_(data_reduction_proxy_settings),
      is_precaching_(false) {
  precache_database_ = std::move(precache_database);
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&PrecacheDatabase::Init),
                 base::Unretained(precache_database_.get()), db_path));
}

PrecacheManager::~PrecacheManager() {
  // DeleteSoon posts a non-nestable task to the task runner, so any previously
  // posted tasks that rely on an Unretained precache_database_ will finish
  // before it is deleted.
  BrowserThread::DeleteSoon(BrowserThread::DB, FROM_HERE,
                            precache_database_.release());
}

bool PrecacheManager::IsInExperimentGroup() const {
  // Verify IsPrecachingAllowed() before calling FieldTrialList::FindFullName().
  // This is because field trials are only assigned when requested. This allows
  // us to create Control and Experiment groups that are limited to users for
  // whom PrecachingAllowed() is true, thus accentuating the impact of
  // precaching.
  return IsPrecachingAllowed() &&
         (base::StartsWith(
              base::FieldTrialList::FindFullName(kPrecacheFieldTrialName),
              kPrecacheFieldTrialEnabledGroup, base::CompareCase::SENSITIVE) ||
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnablePrecache));
}

bool PrecacheManager::IsInControlGroup() const {
  // Verify IsPrecachingAllowed() before calling FindFullName(). See
  // PrecacheManager::IsInExperimentGroup() for an explanation of why.
  return IsPrecachingAllowed() &&
         base::StartsWith(
             base::FieldTrialList::FindFullName(kPrecacheFieldTrialName),
             kPrecacheFieldTrialControlGroup, base::CompareCase::SENSITIVE);
}

bool PrecacheManager::IsPrecachingAllowed() const {
  return PrecachingAllowed() == AllowedType::ALLOWED;
}

PrecacheManager::AllowedType PrecacheManager::PrecachingAllowed() const {
  bool disable_if_proxy = !variations::GetVariationParamValue(
      kPrecacheFieldTrialName, kDataReductionProxyParam).empty();
  if (disable_if_proxy &&
      (!data_reduction_proxy_settings_ ||
       data_reduction_proxy_settings_->IsDataReductionProxyEnabled()))
    return AllowedType::DISALLOWED;

  if (!(sync_service_ && sync_service_->IsEngineInitialized()))
    return AllowedType::PENDING;

  // SyncService delegates to SyncPrefs, which must be called on the UI thread.
  if (history_service_ && !sync_service_->IsLocalSyncEnabled() &&
      sync_service_->GetActiveDataTypes().Has(syncer::SESSIONS) &&
      !sync_service_->GetEncryptedDataTypes().Has(syncer::SESSIONS)) {
    return AllowedType::ALLOWED;
  }

  return AllowedType::DISALLOWED;
}

void PrecacheManager::OnCacheBackendReceived(int net_error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (net_error_code != net::OK) {
    // Assume there is no cache.
    cache_backend_ = nullptr;
    OnCacheSizeReceived(0);
    return;
  }
  DCHECK(cache_backend_);
  int result = cache_backend_->CalculateSizeOfAllEntries(base::Bind(
      &PrecacheManager::OnCacheSizeReceived, base::Unretained(this)));
  if (result == net::ERR_IO_PENDING) {
    // Wait for the callback.
  } else if (result >= 0) {
    // The result is the expected bytes already.
    OnCacheSizeReceived(result);
  } else {
    // Error occurred. Couldn't get the size. Assume there is no cache.
    OnCacheSizeReceived(0);
  }
  cache_backend_ = nullptr;
}

void PrecacheManager::OnCacheSizeReceived(int cache_size_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrecacheManager::OnCacheSizeReceivedInUIThread,
                 base::Unretained(this), cache_size_bytes));
}

void PrecacheManager::OnCacheSizeReceivedInUIThread(int cache_size_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_MEMORY_KB("Precache.CacheSize.AllEntries",
                          cache_size_bytes / 1024);

  if (cache_size_bytes < min_cache_size_bytes_) {
    OnDone();  // Do not continue.
  } else {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::GetUnfinishedWork,
                   base::Unretained(precache_database_.get())),
        base::Bind(&PrecacheManager::OnGetUnfinishedWorkDone, AsWeakPtr()));
  }
}

void PrecacheManager::PrecacheIfCacheIsBigEnough(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(url_request_context_getter);

  // Continue with OnGetUnfinishedWorkDone only if the size of the cache is
  // at least min_cache_size_bytes_.
  // Class disk_cache::Backend does not expose its maximum size. However, caches
  // are usually full, so we can use the size of all the entries stored in the
  // cache (via CalculateSizeOfAllEntries) as a proxy of its maximum size.
  net::URLRequestContext* context =
      url_request_context_getter->GetURLRequestContext();
  if (!context) {
    OnCacheSizeReceived(0);
    return;
  }
  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  if (!factory) {
    OnCacheSizeReceived(0);
    return;
  }
  net::HttpCache* cache = factory->GetCache();
  if (!cache) {
    // There is no known cache. Assume that there is no cache.
    // TODO(jamartin): I'm not sure this can be an actual posibility. Consider
    // making this a CHECK(cache).
    OnCacheSizeReceived(0);
    return;
  }
  const int net_error_code = cache->GetBackend(
      &cache_backend_, base::Bind(&PrecacheManager::OnCacheBackendReceived,
                                  base::Unretained(this)));
  if (net_error_code != net::ERR_IO_PENDING) {
    // No need to wait for the callback. The callback hasn't been called with
    // the appropriate code, so we call it directly.
    OnCacheBackendReceived(net_error_code);
  }
}

void PrecacheManager::StartPrecaching(
    const PrecacheCompletionCallback& precache_completion_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (is_precaching_) {
    DLOG(WARNING) << "Cannot start precaching because precaching is already "
                     "in progress.";
    return;
  }
  precache_completion_callback_ = precache_completion_callback;

  is_precaching_ = true;
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PrecacheDatabase::SetLastPrecacheTimestamp,
                 base::Unretained(precache_database_.get()),
                 base::Time::Now()));

  // Ignore boolean return value. In all documented failure cases, it sets the
  // int to a reasonable value.
  base::StringToInt(variations::GetVariationParamValue(kPrecacheFieldTrialName,
                                                       kMinCacheSizeParam),
                    &min_cache_size_bytes_);
  if (min_cache_size_bytes_ <= 0) {
    // Skip looking up the cache size, because it doesn't matter.
    OnCacheSizeReceivedInUIThread(0);
    return;
  }

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetURLRequestContext());
  if (!url_request_context_getter) {
    OnCacheSizeReceivedInUIThread(0);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PrecacheManager::PrecacheIfCacheIsBigEnough, AsWeakPtr(),
                 std::move(url_request_context_getter)));
}

void PrecacheManager::OnGetUnfinishedWorkDone(
    std::unique_ptr<PrecacheUnfinishedWork> unfinished_work) {
  // Reset progress on a prefetch that has taken too long to complete.
  if (unfinished_work->has_start_time() &&
      base::Time::Now() -
              base::Time::FromInternalValue(unfinished_work->start_time()) >
          base::TimeDelta::FromHours(6)) {
    PrecacheFetcher::RecordCompletionStatistics(
        *unfinished_work, unfinished_work->top_host_size(),
        unfinished_work->resource_size());
    unfinished_work.reset(new PrecacheUnfinishedWork);
  }
  // If this prefetch is new, set the start time.
  if (!unfinished_work->has_start_time())
    unfinished_work->set_start_time(base::Time::Now().ToInternalValue());
  unfinished_work_ = std::move(unfinished_work);
  bool needs_top_hosts = unfinished_work_->top_host_size() == 0;

  base::RecordAction(base::UserMetricsAction("Precache.Fetch.Begin"));

  if (IsInExperimentGroup()) {
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::DeleteExpiredPrecacheHistory,
                   base::Unretained(precache_database_.get()),
                   base::Time::Now()));

    // Request NumTopHosts() top hosts. Note that PrecacheFetcher is further
    // bound by the value of PrecacheConfigurationSettings.top_sites_count, as
    // retrieved from the server.
    if (needs_top_hosts) {
      history_service_->TopHosts(
          NumTopHosts(),
          base::Bind(&PrecacheManager::OnHostsReceived, AsWeakPtr()));
    } else {
      InitializeAndStartFetcher();
    }
  } else if (IsInControlGroup()) {
    // Calculate TopHosts solely for metrics purposes.
    if (needs_top_hosts) {
      history_service_->TopHosts(
          NumTopHosts(),
          base::Bind(&PrecacheManager::OnHostsReceivedThenDone, AsWeakPtr()));
    } else {
      OnDone();
    }
  } else {
    if (PrecachingAllowed() != AllowedType::PENDING) {
      // We are not waiting on the sync engine to be initialized. The user
      // either is not in the field trial, or does not have sync enabled.
      // Pretend that precaching started, so that the PrecacheServiceLauncher
      // doesn't try to start it again.
    }

    OnDone();
  }
}

void PrecacheManager::CancelPrecaching() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!is_precaching_) {
    // Do nothing if precaching is not in progress.
    return;
  }
  is_precaching_ = false;
  // If cancellation occurs after StartPrecaching but before OnHostsReceived,
  // is_precaching will be true, but the precache_fetcher_ will not yet be
  // constructed.
  if (precache_fetcher_) {
    std::unique_ptr<PrecacheUnfinishedWork> unfinished_work =
        precache_fetcher_->CancelPrecaching();
    if (unfinished_work) {
      BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                              base::Bind(&PrecacheDatabase::SaveUnfinishedWork,
                                         precache_database_->GetWeakPtr(),
                                         base::Passed(&unfinished_work)));
    }
    // Destroying the |precache_fetcher_| will cancel any fetch in progress.
    precache_fetcher_.reset();
  }
  precache_completion_callback_.Reset();
}

bool PrecacheManager::IsPrecaching() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_precaching_;
}

void PrecacheManager::UpdatePrecacheMetricsAndState(
    const GURL& url,
    const GURL& referrer,
    const base::Time& fetch_time,
    const net::HttpResponseInfo& info,
    int64_t size,
    bool is_user_traffic,
    const base::Callback<void(base::Time)>& register_synthetic_trial) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PrecacheDatabase::GetLastPrecacheTimestamp,
                 base::Unretained(precache_database_.get())),
      base::Bind(&PrecacheManager::RecordStatsForFetch, AsWeakPtr(), url,
                 referrer, fetch_time, info, size, register_synthetic_trial));

  if (is_user_traffic && IsPrecaching())
    CancelPrecaching();
}

void PrecacheManager::RecordStatsForFetch(
    const GURL& url,
    const GURL& referrer,
    const base::Time& fetch_time,
    const net::HttpResponseInfo& info,
    int64_t size,
    const base::Callback<void(base::Time)>& register_synthetic_trial,
    base::Time last_precache_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  register_synthetic_trial.Run(last_precache_time);

  if (size == 0 || url.is_empty() || !url.SchemeIsHTTPOrHTTPS()) {
    // Ignore empty responses, empty URLs, or URLs that aren't HTTP or HTTPS.
    return;
  }

  if (!history_service_)
    return;

  history_service_->HostRankIfAvailable(
      referrer,
      base::Bind(&PrecacheManager::RecordStatsForFetchInternal, AsWeakPtr(),
                 url, referrer.host(), fetch_time, info, size));
}

void PrecacheManager::RecordStatsForFetchInternal(
    const GURL& url,
    const std::string& referrer_host,
    const base::Time& fetch_time,
    const net::HttpResponseInfo& info,
    int64_t size,
    int host_rank) {
  if (is_precaching_) {
    // Assume that precache is responsible for all requests made while
    // precaching is currently in progress.
    // TODO(sclittle): Make PrecacheFetcher explicitly mark precache-motivated
    // fetches, and use that to determine whether or not a fetch was motivated
    // by precaching.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::RecordURLPrefetchMetrics,
                   base::Unretained(precache_database_.get()), info));
  } else {
    bool is_connection_cellular =
        net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType());

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::RecordURLNonPrefetch,
                   base::Unretained(precache_database_.get()), url, fetch_time,
                   info, size, host_rank, is_connection_cellular));
  }
}

void PrecacheManager::ClearHistory() {
  // PrecacheDatabase::ClearHistory must run after PrecacheDatabase::Init has
  // finished. Using PostNonNestableTask guarantees this, by definition. See
  // base::SequencedTaskRunner for details.
  BrowserThread::PostNonNestableTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PrecacheDatabase::ClearHistory,
                 base::Unretained(precache_database_.get())));
}

void PrecacheManager::Shutdown() {
  CancelPrecaching();
}

void PrecacheManager::OnDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  precache_fetcher_.reset();

  // Run completion callback if not null. It's null if the client is in the
  // Control group and CancelPrecaching is called before TopHosts computation
  // finishes.
  if (!precache_completion_callback_.is_null()) {
    precache_completion_callback_.Run(!is_precaching_);
    // Uninitialize the callback so that any scoped_refptrs in it are released.
    precache_completion_callback_.Reset();
  }

  is_precaching_ = false;
}

void PrecacheManager::OnHostsReceived(
    const history::TopHostsList& host_counts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& host_count : host_counts) {
    TopHost* top_host = unfinished_work_->add_top_host();
    top_host->set_hostname(host_count.first);
    top_host->set_visits(host_count.second);
  }
  InitializeAndStartFetcher();
}

void PrecacheManager::InitializeAndStartFetcher() {
  if (!is_precaching_) {
    // Don't start precaching if it was canceled while waiting for the list of
    // hosts.
    return;
  }
  // Start precaching.
  precache_fetcher_.reset(new PrecacheFetcher(
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetURLRequestContext(),
      GURL(variations::GetVariationParamValue(kPrecacheFieldTrialName,
                                              kConfigURLParam)),
      variations::GetVariationParamValue(kPrecacheFieldTrialName,
                                         kManifestURLPrefixParam),
      std::move(unfinished_work_),
      metrics::HashName(
          base::FieldTrialList::FindFullName(kPrecacheFieldTrialName)),
      precache_database_->GetWeakPtr(),
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::DB),
      this));
  precache_fetcher_->Start();
}

void PrecacheManager::OnHostsReceivedThenDone(
    const history::TopHostsList& host_counts) {
  OnDone();
}

}  // namespace precache
