// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/preconnect_manager.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_hints.h"
#include "net/base/net_errors.h"

namespace predictors {

const bool kAllowCredentialsOnPreconnectByDefault = true;

PreconnectedRequestStats::PreconnectedRequestStats(const GURL& origin,
                                                   bool was_preresolve_cached,
                                                   bool was_preconnected)
    : origin(origin),
      was_preresolve_cached(was_preresolve_cached),
      was_preconnected(was_preconnected) {}

PreconnectedRequestStats::PreconnectedRequestStats(
    const PreconnectedRequestStats& other) = default;
PreconnectedRequestStats::~PreconnectedRequestStats() = default;

PreconnectStats::PreconnectStats(const GURL& url)
    : url(url), start_time(base::TimeTicks::Now()) {}
PreconnectStats::~PreconnectStats() = default;

PreresolveInfo::PreresolveInfo(const GURL& url, size_t count)
    : url(url),
      queued_count(count),
      inflight_count(0),
      was_canceled(false),
      stats(std::make_unique<PreconnectStats>(url)) {}

PreresolveInfo::~PreresolveInfo() = default;

PreresolveJob::PreresolveJob(const GURL& url,
                             bool need_preconnect,
                             bool allow_credentials,
                             PreresolveInfo* info)
    : url(url),
      need_preconnect(need_preconnect),
      allow_credentials(allow_credentials),
      info(info) {}

PreresolveJob::PreresolveJob(const PreresolveJob& other) = default;
PreresolveJob::~PreresolveJob() = default;

PreconnectManager::PreconnectManager(
    base::WeakPtr<Delegate> delegate,
    scoped_refptr<net::URLRequestContextGetter> context_getter)
    : delegate_(std::move(delegate)),
      context_getter_(std::move(context_getter)),
      inflight_preresolves_count_(0),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context_getter_);
}

PreconnectManager::~PreconnectManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void PreconnectManager::Start(const GURL& url,
                              const std::vector<GURL>& preconnect_origins,
                              const std::vector<GURL>& preresolve_hosts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const std::string host = url.host();
  if (preresolve_info_.find(host) != preresolve_info_.end())
    return;

  auto iterator_and_whether_inserted = preresolve_info_.emplace(
      host, base::MakeUnique<PreresolveInfo>(
                url, preconnect_origins.size() + preresolve_hosts.size()));
  PreresolveInfo* info = iterator_and_whether_inserted.first->second.get();

  for (const GURL& origin : preconnect_origins) {
    DCHECK(origin.GetOrigin() == origin);
    queued_jobs_.emplace_back(origin, true /* need_preconnect */,
                              kAllowCredentialsOnPreconnectByDefault, info);
  }

  for (const GURL& host : preresolve_hosts) {
    DCHECK(host.GetOrigin() == host);
    queued_jobs_.emplace_back(host, false /* need_preconnect */,
                              kAllowCredentialsOnPreconnectByDefault, info);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreresolveHost(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  queued_jobs_.emplace_front(url.GetOrigin(), false /* need_preconnect */,
                             kAllowCredentialsOnPreconnectByDefault, nullptr);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreresolveHosts(
    const std::vector<std::string>& hostnames) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Push jobs in front of the queue due to higher priority.
  for (auto it = hostnames.rbegin(); it != hostnames.rend(); ++it) {
    queued_jobs_.emplace_front(GURL("http://" + *it),
                               false /* need_preconnect */,
                               kAllowCredentialsOnPreconnectByDefault, nullptr);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreconnectUrl(const GURL& url,
                                           bool allow_credentials) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  queued_jobs_.emplace_front(url.GetOrigin(), true /* need_preconnect */,
                             allow_credentials, nullptr);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::Stop(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto it = preresolve_info_.find(url.host());
  if (it == preresolve_info_.end()) {
    return;
  }

  it->second->was_canceled = true;
}

void PreconnectManager::PreconnectUrl(const GURL& url,
                                      const GURL& site_for_cookies,
                                      bool allow_credentials) const {
  DCHECK(url.GetOrigin() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  content::PreconnectUrl(context_getter_.get(), url, site_for_cookies, 1,
                         allow_credentials,
                         net::HttpRequestInfo::PRECONNECT_MOTIVATED);
}

int PreconnectManager::PreresolveUrl(
    const GURL& url,
    const net::CompletionCallback& callback) const {
  DCHECK(url.GetOrigin() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  return content::PreresolveUrl(context_getter_.get(), url, callback);
}

void PreconnectManager::TryToLaunchPreresolveJobs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  while (!queued_jobs_.empty() &&
         inflight_preresolves_count_ < kMaxInflightPreresolves) {
    auto& job = queued_jobs_.front();
    PreresolveInfo* info = job.info;

    if (!info || !info->was_canceled) {
      int status = PreresolveUrl(
          job.url, base::Bind(&PreconnectManager::OnPreresolveFinished,
                              weak_factory_.GetWeakPtr(), job));
      if (status == net::ERR_IO_PENDING) {
        // Will complete asynchronously.
        if (info)
          ++info->inflight_count;
        ++inflight_preresolves_count_;
      } else {
        // Completed synchronously (was already cached by HostResolver), or else
        // there was (equivalently) some network error that prevents us from
        // finding the name. Status net::OK means it was "found."
        FinishPreresolve(job, status == net::OK, true);
      }
    }

    queued_jobs_.pop_front();
    if (info)
      --info->queued_count;
    if (info && info->is_done())
      AllPreresolvesForUrlFinished(info);
  }
}

void PreconnectManager::OnPreresolveFinished(const PreresolveJob& job,
                                             int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  FinishPreresolve(job, result == net::OK, false);
  PreresolveInfo* info = job.info;
  --inflight_preresolves_count_;
  if (info)
    --info->inflight_count;
  if (info && info->is_done())
    AllPreresolvesForUrlFinished(info);
  TryToLaunchPreresolveJobs();
}

void PreconnectManager::FinishPreresolve(const PreresolveJob& job,
                                         bool found,
                                         bool cached) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  PreresolveInfo* info = job.info;
  bool need_preconnect =
      found && job.need_preconnect && (!info || !info->was_canceled);
  if (need_preconnect)
    PreconnectUrl(job.url, info ? info->url : GURL(), job.allow_credentials);
  if (info && found)
    info->stats->requests_stats.emplace_back(job.url, cached, need_preconnect);
}

void PreconnectManager::AllPreresolvesForUrlFinished(PreresolveInfo* info) {
  DCHECK(info);
  DCHECK(info->is_done());
  auto it = preresolve_info_.find(info->url.host());
  DCHECK(it != preresolve_info_.end());
  DCHECK(info == it->second.get());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&Delegate::PreconnectFinished, delegate_,
                     std::move(info->stats)));
  preresolve_info_.erase(it);
}

}  // namespace predictors
