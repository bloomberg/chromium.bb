// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_quota_helper.h"

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo()
    : temporary_usage(0),
      persistent_usage(0) {}

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo(const std::string& host)
    : host(host),
      temporary_usage(0),
      persistent_usage(0) {}

BrowsingDataQuotaHelper::QuotaInfo::QuotaInfo(const std::string& host,
                                              int64 temporary_usage,
                                              int64 persistent_usage)
    : host(host),
      temporary_usage(temporary_usage),
      persistent_usage(persistent_usage) {}

BrowsingDataQuotaHelper::QuotaInfo::~QuotaInfo() {}

// static
void BrowsingDataQuotaHelperDeleter::Destruct(
    const BrowsingDataQuotaHelper* helper) {
  helper->io_thread_->DeleteSoon(FROM_HERE, helper);
}

BrowsingDataQuotaHelper::BrowsingDataQuotaHelper(
    base::MessageLoopProxy* io_thread)
    : io_thread_(io_thread) {
}

BrowsingDataQuotaHelper::~BrowsingDataQuotaHelper() {
}

bool operator <(const BrowsingDataQuotaHelper::QuotaInfo& lhs,
                const BrowsingDataQuotaHelper::QuotaInfo& rhs) {
  if (lhs.host != rhs.host)
    return lhs.host < rhs.host;
  if (lhs.temporary_usage != rhs.temporary_usage)
    return lhs.temporary_usage < rhs.temporary_usage;
  return lhs.persistent_usage < rhs.persistent_usage;
}

bool operator ==(const BrowsingDataQuotaHelper::QuotaInfo& lhs,
                 const BrowsingDataQuotaHelper::QuotaInfo& rhs) {
  return lhs.host == rhs.host &&
      lhs.temporary_usage == rhs.temporary_usage &&
      lhs.persistent_usage == rhs.persistent_usage;
}
