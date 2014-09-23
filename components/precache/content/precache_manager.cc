// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/content/precache_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/precache/core/precache_database.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/url_list_provider.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

using content::BrowserThread;

namespace {

const char kPrecacheFieldTrialName[] = "Precache";
const char kPrecacheFieldTrialEnabledGroup[] = "Enabled";

}  // namespace

namespace precache {

PrecacheManager::PrecacheManager(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
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

// static
bool PrecacheManager::IsPrecachingEnabled() {
  return base::FieldTrialList::FindFullName(kPrecacheFieldTrialName) ==
             kPrecacheFieldTrialEnabledGroup ||
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePrecache);
}

bool PrecacheManager::IsPrecachingAllowed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return user_prefs::UserPrefs::Get(browser_context_)->GetBoolean(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled);
}

void PrecacheManager::StartPrecaching(
    const PrecacheCompletionCallback& precache_completion_callback,
    URLListProvider* url_list_provider) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (is_precaching_) {
    DLOG(WARNING) << "Cannot start precaching because precaching is already "
                     "in progress.";
    return;
  }
  is_precaching_ = true;

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PrecacheDatabase::DeleteExpiredPrecacheHistory,
                 precache_database_, base::Time::Now()));

  precache_completion_callback_ = precache_completion_callback;

  url_list_provider->GetURLs(
      base::Bind(&PrecacheManager::OnURLsReceived, AsWeakPtr()));
}

void PrecacheManager::CancelPrecaching() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_precaching_;
}

void PrecacheManager::RecordStatsForFetch(const GURL& url,
                                          const base::Time& fetch_time,
                                          int64 size,
                                          bool was_cached) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

void PrecacheManager::Shutdown() {
  CancelPrecaching();
}

void PrecacheManager::OnDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If OnDone has been called, then we should just be finishing precaching.
  DCHECK(is_precaching_);
  is_precaching_ = false;

  precache_fetcher_.reset();

  precache_completion_callback_.Run();
  // Uninitialize the callback so that any scoped_refptrs in it are released.
  precache_completion_callback_.Reset();
}

void PrecacheManager::OnURLsReceived(const std::list<GURL>& urls) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!is_precaching_) {
    // Don't start precaching if it was canceled while waiting for the list of
    // URLs.
    return;
  }

  // Start precaching.
  precache_fetcher_.reset(
      new PrecacheFetcher(urls, browser_context_->GetRequestContext(), this));
  precache_fetcher_->Start();
}

}  // namespace precache
