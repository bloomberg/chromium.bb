// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/content/precache_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/precache/core/precache_database.h"
#include "components/precache/core/precache_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

using content::BrowserThread;

namespace {

const char kPrecacheFieldTrialName[] = "Precache";
const char kPrecacheFieldTrialEnabledGroup[] = "Enabled";
const char kPrecacheFieldTrialControlGroup[] = "Control";
const char kConfigURLParam[] = "config_url";
const char kManifestURLPrefixParam[] = "manifest_url_prefix";
const size_t kNumTopHosts = 100;

}  // namespace

namespace precache {

size_t NumTopHosts() {
  return kNumTopHosts;
}

PrecacheManager::PrecacheManager(
    content::BrowserContext* browser_context,
    const sync_driver::SyncService* const sync_service,
    const history::HistoryService* const history_service)
    : browser_context_(browser_context),
      sync_service_(sync_service),
      history_service_(history_service),
      precache_database_(new PrecacheDatabase()),
      is_precaching_(false) {
  base::FilePath db_path(browser_context_->GetPath().Append(
      base::FilePath(FILE_PATH_LITERAL("PrecacheDatabase"))));

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&PrecacheDatabase::Init),
                 precache_database_, db_path));
}

PrecacheManager::~PrecacheManager() {}

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
  if (!(sync_service_ && sync_service_->IsBackendInitialized()))
    return AllowedType::PENDING;

  // SyncService delegates to SyncPrefs, which must be called on the UI thread.
  if (history_service_ &&
      sync_service_->GetActiveDataTypes().Has(syncer::SESSIONS) &&
      !sync_service_->GetEncryptedDataTypes().Has(syncer::SESSIONS))
    return AllowedType::ALLOWED;

  return AllowedType::DISALLOWED;
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

  if (IsInExperimentGroup()) {
    is_precaching_ = true;

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::DeleteExpiredPrecacheHistory,
                   precache_database_, base::Time::Now()));

    // Request NumTopHosts() top hosts. Note that PrecacheFetcher is further
    // bound by the value of PrecacheConfigurationSettings.top_sites_count, as
    // retrieved from the server.
    history_service_->TopHosts(
        NumTopHosts(),
        base::Bind(&PrecacheManager::OnHostsReceived, AsWeakPtr()));
  } else if (IsInControlGroup()) {
    // Set is_precaching_ so that the longer delay is placed between calls to
    // TopHosts.
    is_precaching_ = true;

    // Calculate TopHosts solely for metrics purposes.
    history_service_->TopHosts(
        NumTopHosts(),
        base::Bind(&PrecacheManager::OnHostsReceivedThenDone, AsWeakPtr()));
  } else {
    if (PrecachingAllowed() != AllowedType::PENDING) {
      // We are not waiting on the sync backend to be initialized. The user
      // either is not in the field trial, or does not have sync enabled.
      // Pretend that precaching started, so that the PrecacheServiceLauncher
      // doesn't try to start it again.
      is_precaching_ = true;
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

  // Destroying the |precache_fetcher_| will cancel any fetch in progress.
  precache_fetcher_.reset();

  // Uninitialize the callback so that any scoped_refptrs in it are released.
  precache_completion_callback_.Reset();
}

bool PrecacheManager::IsPrecaching() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_precaching_;
}

void PrecacheManager::RecordStatsForFetch(const GURL& url,
                                          const GURL& referrer,
                                          const base::TimeDelta& latency,
                                          const base::Time& fetch_time,
                                          int64_t size,
                                          bool was_cached) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (size == 0 || url.is_empty() || !url.SchemeIsHTTPOrHTTPS()) {
    // Ignore empty responses, empty URLs, or URLs that aren't HTTP or HTTPS.
    return;
  }

  if (!history_service_)
    return;

  history_service_->HostRankIfAvailable(
      referrer,
      base::Bind(&PrecacheManager::RecordStatsForFetchInternal, AsWeakPtr(),
                 url, latency, fetch_time, size, was_cached));
}

void PrecacheManager::RecordStatsForFetchInternal(
    const GURL& url,
    const base::TimeDelta& latency,
    const base::Time& fetch_time,
    int64_t size,
    bool was_cached,
    int host_rank) {
  if (is_precaching_) {
    // Assume that precache is responsible for all requests made while
    // precaching is currently in progress.
    // TODO(sclittle): Make PrecacheFetcher explicitly mark precache-motivated
    // fetches, and use that to determine whether or not a fetch was motivated
    // by precaching.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::RecordURLPrefetch, precache_database_,
                   url, latency, fetch_time, size, was_cached));
  } else {
    bool is_connection_cellular =
        net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType());

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::RecordURLNonPrefetch, precache_database_,
                   url, latency, fetch_time, size, was_cached, host_rank,
                   is_connection_cellular));
  }
}

void PrecacheManager::ClearHistory() {
  // PrecacheDatabase::ClearHistory must run after PrecacheDatabase::Init has
  // finished. Using PostNonNestableTask guarantees this, by definition. See
  // base::SequencedTaskRunner for details.
  BrowserThread::PostNonNestableTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PrecacheDatabase::ClearHistory, precache_database_));
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

  if (!is_precaching_) {
    // Don't start precaching if it was canceled while waiting for the list of
    // hosts.
    return;
  }

  std::vector<std::string> hosts;
  for (const auto& host_count : host_counts)
    hosts.push_back(host_count.first);

  // Start precaching.
  precache_fetcher_.reset(
      new PrecacheFetcher(hosts, browser_context_->GetRequestContext(),
                          GURL(variations::GetVariationParamValue(
                              kPrecacheFieldTrialName, kConfigURLParam)),
                          variations::GetVariationParamValue(
                              kPrecacheFieldTrialName, kManifestURLPrefixParam),
                          this));
  precache_fetcher_->Start();
}

void PrecacheManager::OnHostsReceivedThenDone(
    const history::TopHostsList& host_counts) {
  OnDone();
}

}  // namespace precache
