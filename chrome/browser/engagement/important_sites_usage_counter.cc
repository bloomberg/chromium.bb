// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/important_sites_usage_counter.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"

using ImportantDomainInfo = ImportantSitesUtil::ImportantDomainInfo;

// static
void ImportantSitesUsageCounter::GetUsage(
    std::vector<ImportantSitesUtil::ImportantDomainInfo> sites,
    storage::QuotaManager* quota_manager,
    content::DOMStorageContext* dom_storage_context,
    UsageCallback callback) {
  (new ImportantSitesUsageCounter(std::move(sites), quota_manager,
                                  dom_storage_context, std::move(callback)))
      ->RunAndDestroySelfWhenFinished();
}

ImportantSitesUsageCounter::ImportantSitesUsageCounter(
    std::vector<ImportantDomainInfo> sites,
    storage::QuotaManager* quota_manager,
    content::DOMStorageContext* dom_storage_context,
    UsageCallback callback)
    : callback_(std::move(callback)),
      sites_(std::move(sites)),
      quota_manager_(quota_manager),
      dom_storage_context_(dom_storage_context),
      tasks_(0) {
  for (ImportantDomainInfo& site : sites_)
    site.usage = 0;
}

ImportantSitesUsageCounter::~ImportantSitesUsageCounter() {}

void ImportantSitesUsageCounter::RunAndDestroySelfWhenFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  tasks_ += 1;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ImportantSitesUsageCounter::GetQuotaUsageOnIOThread,
                     base::Unretained(this)));
  tasks_ += 1;
  dom_storage_context_->GetLocalStorageUsage(
      base::Bind(&ImportantSitesUsageCounter::ReceiveLocalStorageUsage,
                 base::Unretained(this)));
}

void ImportantSitesUsageCounter::GetQuotaUsageOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  quota_manager_->GetUsageInfo(
      base::Bind(&ImportantSitesUsageCounter::ReceiveQuotaUsageOnIOThread,
                 base::Unretained(this)));
}

void ImportantSitesUsageCounter::ReceiveQuotaUsageOnIOThread(
    const std::vector<storage::UsageInfo>& usage_infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ImportantSitesUsageCounter::ReceiveQuotaUsage,
                     base::Unretained(this), usage_infos));
}

void ImportantSitesUsageCounter::ReceiveQuotaUsage(
    const std::vector<storage::UsageInfo>& usage_infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const storage::UsageInfo& info : usage_infos) {
    IncrementUsage(
        ImportantSitesUtil::GetRegisterableDomainOrIPFromHost(info.host),
        info.usage);
  }
  Done();
}

void ImportantSitesUsageCounter::ReceiveLocalStorageUsage(
    const std::vector<content::LocalStorageUsageInfo>& storage_infos) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const content::LocalStorageUsageInfo& info : storage_infos) {
    IncrementUsage(ImportantSitesUtil::GetRegisterableDomainOrIP(info.origin),
                   info.data_size);
  }
  Done();
}

// Look up the corresponding ImportantDomainInfo for |url| and increase its
// usage by |size|.
void ImportantSitesUsageCounter::IncrementUsage(const std::string& domain,
                                                int64_t size) {
  // Use a linear search over sites_ because it only has up to 10 entries.
  auto it = std::find_if(sites_.begin(), sites_.end(),
                         [domain](ImportantDomainInfo& info) {
                           return info.registerable_domain == domain;
                         });
  if (it != sites_.end())
    it->usage += size;
}

void ImportantSitesUsageCounter::Done() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_GT(tasks_, 0);
  if (--tasks_ == 0) {
    std::move(callback_).Run(std::move(sites_));
    delete this;
  }
}
