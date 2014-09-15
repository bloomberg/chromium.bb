// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"

using content::BrowserThread;
using content::ServiceWorkerContext;
using content::ServiceWorkerUsageInfo;

BrowsingDataServiceWorkerHelper::BrowsingDataServiceWorkerHelper(
    ServiceWorkerContext* service_worker_context)
    : service_worker_context_(service_worker_context), is_fetching_(false) {
  DCHECK(service_worker_context_);
}

BrowsingDataServiceWorkerHelper::~BrowsingDataServiceWorkerHelper() {
}

void BrowsingDataServiceWorkerHelper::StartFetching(const base::Callback<
    void(const std::list<ServiceWorkerUsageInfo>&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_fetching_);
  DCHECK(!callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&BrowsingDataServiceWorkerHelper::
                                         FetchServiceWorkerUsageInfoOnIOThread,
                                     this));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnIOThread,
          this,
          origin));
}

void BrowsingDataServiceWorkerHelper::FetchServiceWorkerUsageInfoOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetAllOriginsInfo(base::Bind(
      &BrowsingDataServiceWorkerHelper::GetAllOriginsInfoCallback, this));
}

void BrowsingDataServiceWorkerHelper::GetAllOriginsInfoCallback(
    const std::vector<ServiceWorkerUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (std::vector<ServiceWorkerUsageInfo>::const_iterator iter =
           origins.begin();
       iter != origins.end();
       ++iter) {
    const ServiceWorkerUsageInfo& origin = *iter;
    if (!BrowsingDataHelper::HasWebScheme(origin.origin))
      continue;  // Non-websafe state is not considered browsing data.

    service_worker_info_.push_back(origin);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&BrowsingDataServiceWorkerHelper::NotifyOnUIThread, this));
}

void BrowsingDataServiceWorkerHelper::NotifyOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(is_fetching_);
  completion_callback_.Run(service_worker_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnIOThread(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->DeleteForOrigin(origin);
}

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    PendingServiceWorkerUsageInfo(const GURL& origin,
                                  const std::vector<GURL>& scopes)
    : origin(origin), scopes(scopes) {
}

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    ~PendingServiceWorkerUsageInfo() {
}

bool CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
operator<(const PendingServiceWorkerUsageInfo& other) const {
  if (origin == other.origin)
    return scopes < other.scopes;
  return origin < other.origin;
}

CannedBrowsingDataServiceWorkerHelper::CannedBrowsingDataServiceWorkerHelper(
    content::ServiceWorkerContext* context)
    : BrowsingDataServiceWorkerHelper(context) {
}

CannedBrowsingDataServiceWorkerHelper::
    ~CannedBrowsingDataServiceWorkerHelper() {
}

void CannedBrowsingDataServiceWorkerHelper::AddServiceWorker(
    const GURL& origin, const std::vector<GURL>& scopes) {
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  pending_service_worker_info_.insert(
      PendingServiceWorkerUsageInfo(origin, scopes));
}

void CannedBrowsingDataServiceWorkerHelper::Reset() {
  service_worker_info_.clear();
  pending_service_worker_info_.clear();
}

bool CannedBrowsingDataServiceWorkerHelper::empty() const {
  return service_worker_info_.empty() && pending_service_worker_info_.empty();
}

size_t CannedBrowsingDataServiceWorkerHelper::GetServiceWorkerCount() const {
  return pending_service_worker_info_.size();
}

const std::set<
    CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo>&
CannedBrowsingDataServiceWorkerHelper::GetServiceWorkerUsageInfo() const {
  return pending_service_worker_info_;
}

void CannedBrowsingDataServiceWorkerHelper::StartFetching(const base::Callback<
    void(const std::list<ServiceWorkerUsageInfo>&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<ServiceWorkerUsageInfo> result;
  for (std::set<PendingServiceWorkerUsageInfo>::const_iterator pending_info =
           pending_service_worker_info_.begin();
       pending_info != pending_service_worker_info_.end();
       ++pending_info) {
    ServiceWorkerUsageInfo info(
        pending_info->origin, pending_info->scopes);
    result.push_back(info);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
}

void CannedBrowsingDataServiceWorkerHelper::DeleteServiceWorkers(
    const GURL& origin) {
  for (std::set<PendingServiceWorkerUsageInfo>::iterator it =
           pending_service_worker_info_.begin();
       it != pending_service_worker_info_.end();) {
    if (it->origin == origin)
      pending_service_worker_info_.erase(it++);
    else
      ++it;
  }
  BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(origin);
}
