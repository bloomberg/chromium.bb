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

const int kAllowCredentialsOnPreconnectByDefault = true;

PreresolveInfo::PreresolveInfo(const GURL& in_url, size_t count)
    : url(in_url),
      queued_count(count),
      inflight_count(0),
      was_canceled(false) {}

PreresolveInfo::PreresolveInfo(const PreresolveInfo& other) = default;
PreresolveInfo::~PreresolveInfo() = default;

PreresolveJob::PreresolveJob(const GURL& in_url,
                             bool in_need_preconnect,
                             PreresolveInfo* in_info)
    : url(in_url), need_preconnect(in_need_preconnect), info(in_info) {}

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
  if (preresolve_info_.find(url) != preresolve_info_.end())
    return;

  auto iterator_and_whether_inserted = preresolve_info_.emplace(
      url, base::MakeUnique<PreresolveInfo>(
               url, preconnect_origins.size() + preresolve_hosts.size()));
  PreresolveInfo* info = iterator_and_whether_inserted.first->second.get();

  for (const GURL& origin : preconnect_origins)
    queued_jobs_.emplace_back(origin, true, info);

  for (const GURL& host : preresolve_hosts)
    queued_jobs_.emplace_back(host, false, info);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::Stop(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto it = preresolve_info_.find(url);
  if (it == preresolve_info_.end()) {
    return;
  }

  it->second->was_canceled = true;
}

void PreconnectManager::PreconnectUrl(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  content::PreconnectUrl(context_getter_.get(), url, first_party_for_cookies, 1,
                         kAllowCredentialsOnPreconnectByDefault,
                         net::HttpRequestInfo::PRECONNECT_MOTIVATED);
}

int PreconnectManager::PreresolveUrl(
    const GURL& url,
    const net::CompletionCallback& callback) const {
  return content::PreresolveUrl(context_getter_.get(), url, callback);
}

void PreconnectManager::TryToLaunchPreresolveJobs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  while (!queued_jobs_.empty() &&
         inflight_preresolves_count_ < kMaxInflightPreresolves) {
    auto& job = queued_jobs_.front();
    PreresolveInfo* info = job.info;

    if (!info->was_canceled) {
      int status = PreresolveUrl(
          job.url, base::Bind(&PreconnectManager::OnPreresolveFinished,
                              weak_factory_.GetWeakPtr(), job));
      if (status == net::ERR_IO_PENDING) {
        // Will complete asynchronously.
        ++info->inflight_count;
        ++inflight_preresolves_count_;
      } else {
        // Completed synchronously (was already cached by HostResolver), or else
        // there was (equivalently) some network error that prevents us from
        // finding the name. Status net::OK means it was "found."
        FinishPreresolve(job, status == net::OK);
      }
    }

    queued_jobs_.pop_front();
    --info->queued_count;
    if (info->is_done())
      AllPreresolvesForUrlFinished(info);
  }
}

void PreconnectManager::OnPreresolveFinished(const PreresolveJob& job,
                                             int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  FinishPreresolve(job, result == net::OK);
  --inflight_preresolves_count_;
  --job.info->inflight_count;
  if (job.info->is_done())
    AllPreresolvesForUrlFinished(job.info);
  TryToLaunchPreresolveJobs();
}

void PreconnectManager::FinishPreresolve(const PreresolveJob& job, bool found) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (found && job.need_preconnect && !job.info->was_canceled) {
    PreconnectUrl(job.url, job.info->url);
  }
}

void PreconnectManager::AllPreresolvesForUrlFinished(PreresolveInfo* info) {
  DCHECK(info->is_done());
  auto it = preresolve_info_.find(info->url);
  DCHECK(it != preresolve_info_.end());
  DCHECK(info == it->second.get());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&Delegate::PreconnectFinished, delegate_, info->url));
  preresolve_info_.erase(it);
}

}  // namespace predictors
