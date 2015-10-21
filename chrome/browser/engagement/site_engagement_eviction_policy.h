// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_EVICTION_POLICY_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_EVICTION_POLICY_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

class SiteEngagementScoreProvider;

class SiteEngagementEvictionPolicy : public storage::QuotaEvictionPolicy {
 public:
  explicit SiteEngagementEvictionPolicy(
      content::BrowserContext* browser_context);
  ~SiteEngagementEvictionPolicy() override;

  // Overridden from storage::QuotaEvictionPolicy:
  void GetEvictionOrigin(const scoped_refptr<storage::SpecialStoragePolicy>&
                             special_storage_policy,
                         const std::set<GURL>& exceptions,
                         const std::map<GURL, int64>& usage_map,
                         int64 global_quota,
                         const storage::GetOriginCallback& callback) override;

 private:
  friend class SiteEngagementEvictionPolicyTest;

  static GURL CalculateEvictionOriginForTests(
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      SiteEngagementScoreProvider* score_provider,
      const std::set<GURL>& exceptions,
      const std::map<GURL, int64>& usage_map,
      int64 global_quota);

  content::BrowserContext* const browser_context_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementEvictionPolicy);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_EVICTION_POLICY_H_
