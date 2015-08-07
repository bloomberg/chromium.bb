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
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/precache/core/precache_database.h"
#include "components/precache/core/precache_switches.h"
#include "components/sync_driver/sync_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

using content::BrowserThread;

namespace {

const char kPrecacheFieldTrialName[] = "Precache";
const char kPrecacheFieldTrialEnabledGroup[] = "Enabled";
const char kManifestURLPrefixParam[] = "manifest_url_prefix";
const int kNumTopHosts = 100;

}  // namespace

namespace precache {

int NumTopHosts() {
  return kNumTopHosts;
}

PrecacheManager::PrecacheManager(
    content::BrowserContext* browser_context,
    const sync_driver::SyncService* const sync_service)
    : browser_context_(browser_context),
      sync_service_(sync_service),
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

bool PrecacheManager::ShouldRun() const {
  // Verify IsPrecachingAllowed() before calling IsPrecachingEnabled(). This is
  // because field trials are only assigned when requested. This allows us to
  // create Control and Experiment groups that are limited to users for whom
  // IsPrecachingAllowed() is true, thus accentuating the impact of precaching.
  return IsPrecachingAllowed() && IsPrecachingEnabled();
}

bool PrecacheManager::WouldRun() const {
  return IsPrecachingAllowed();
}

// static
bool PrecacheManager::IsPrecachingEnabled() {
  return base::FieldTrialList::FindFullName(kPrecacheFieldTrialName) ==
             kPrecacheFieldTrialEnabledGroup ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnablePrecache);
}

bool PrecacheManager::IsPrecachingAllowed() const {
  // SyncService delegates to SyncPrefs, which must be called on the UI thread.
  return sync_service_ &&
         sync_service_->GetActiveDataTypes().Has(syncer::SESSIONS) &&
         !sync_service_->GetEncryptedDataTypes().Has(syncer::SESSIONS);
}

void PrecacheManager::StartPrecaching(
    const PrecacheCompletionCallback& precache_completion_callback,
    const history::HistoryService& history_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (is_precaching_) {
    DLOG(WARNING) << "Cannot start precaching because precaching is already "
                     "in progress.";
    return;
  }
  is_precaching_ = true;

  precache_completion_callback_ = precache_completion_callback;

  if (ShouldRun()) {
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::DeleteExpiredPrecacheHistory,
                   precache_database_, base::Time::Now()));

    // Request NumTopHosts() top hosts. Note that PrecacheFetcher is further
    // bound by the value of PrecacheConfigurationSettings.top_sites_count, as
    // retrieved from the server.
    history_service.TopHosts(
        NumTopHosts(),
        base::Bind(&PrecacheManager::OnHostsReceived, AsWeakPtr()));
  } else {
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
                                          const base::Time& fetch_time,
                                          int64 size,
                                          bool was_cached) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (size == 0 || url.is_empty() || !url.SchemeIsHTTPOrHTTPS()) {
    // Ignore empty responses, empty URLs, or URLs that aren't HTTP or HTTPS.
    return;
  }

  if (is_precaching_) {
    // Assume that precache is responsible for all requests made while
    // precaching is currently in progress.
    // TODO(sclittle): Make PrecacheFetcher explicitly mark precache-motivated
    // fetches, and use that to determine whether or not a fetch was motivated
    // by precaching.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::RecordURLPrecached, precache_database_,
                   url, fetch_time, size, was_cached));
  } else {
    bool is_connection_cellular =
        net::NetworkChangeNotifier::IsConnectionCellular(
            net::NetworkChangeNotifier::GetConnectionType());

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&PrecacheDatabase::RecordURLFetched, precache_database_, url,
                   fetch_time, size, was_cached, is_connection_cellular));
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

  // If OnDone has been called, then we should just be finishing precaching.
  DCHECK(is_precaching_);
  is_precaching_ = false;

  precache_fetcher_.reset();

  precache_completion_callback_.Run();
  // Uninitialize the callback so that any scoped_refptrs in it are released.
  precache_completion_callback_.Reset();
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
                          variations::GetVariationParamValue(
                              kPrecacheFieldTrialName, kManifestURLPrefixParam),
                          this));
  precache_fetcher_->Start();
}

}  // namespace precache
