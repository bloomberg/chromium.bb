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

bool BrowsingDataQuotaHelper::QuotaInfo::operator <(
    const BrowsingDataQuotaHelper::QuotaInfo& rhs) const {
  if (this->host != rhs.host)
    return this->host < rhs.host;
  if (this->temporary_usage != rhs.temporary_usage)
    return this->temporary_usage < rhs.temporary_usage;
  return this->persistent_usage < rhs.persistent_usage;
}

bool BrowsingDataQuotaHelper::QuotaInfo::operator ==(
    const BrowsingDataQuotaHelper::QuotaInfo& rhs) const {
  return this->host == rhs.host &&
      this->temporary_usage == rhs.temporary_usage &&
      this->persistent_usage == rhs.persistent_usage;
}
