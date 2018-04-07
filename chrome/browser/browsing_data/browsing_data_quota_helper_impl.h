// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_IMPL_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

class GURL;

namespace storage {
class QuotaManager;
}

// Implementation of BrowsingDataQuotaHelper.  Since a client of
// BrowsingDataQuotaHelper should live in UI thread and QuotaManager lives in
// IO thread, we have to communicate over thread using PostTask.
class BrowsingDataQuotaHelperImpl : public BrowsingDataQuotaHelper {
 public:
  void StartFetching(const FetchResultCallback& callback) override;
  void RevokeHostQuota(const std::string& host) override;

 private:
  using PendingHosts =
      std::set<std::pair<std::string, blink::mojom::StorageType>>;
  using QuotaInfoMap = std::map<std::string, QuotaInfo>;

  explicit BrowsingDataQuotaHelperImpl(storage::QuotaManager* quota_manager);
  ~BrowsingDataQuotaHelperImpl() override;

  // Calls QuotaManager::GetOriginModifiedSince for each storage type.
  void FetchQuotaInfoOnIOThread(const FetchResultCallback& callback);

  // Callback function for QuotaManager::GetOriginModifiedSince.
  void GotOrigins(PendingHosts* pending_hosts,
                  const base::Closure& completion,
                  const std::set<GURL>& origins,
                  blink::mojom::StorageType type);

  // Calls QuotaManager::GetHostUsage for each (origin, type) pair.
  void OnGetOriginsComplete(const FetchResultCallback& callback,
                            PendingHosts* pending_hosts);

  // Callback function for QuotaManager::GetHostUsage.
  void GotHostUsage(QuotaInfoMap* quota_info,
                    const base::Closure& completion,
                    const std::string& host,
                    blink::mojom::StorageType type,
                    int64_t usage);

  // Called when all QuotaManager::GetHostUsage requests are complete.
  void OnGetHostsUsageComplete(const FetchResultCallback& callback,
                               QuotaInfoMap* quota_info);

  void RevokeHostQuotaOnIOThread(const std::string& host);
  void DidRevokeHostQuota(blink::mojom::QuotaStatusCode status, int64_t quota);

  scoped_refptr<storage::QuotaManager> quota_manager_;

  base::WeakPtrFactory<BrowsingDataQuotaHelperImpl> weak_factory_;

  friend class BrowsingDataQuotaHelper;
  friend class BrowsingDataQuotaHelperTest;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataQuotaHelperImpl);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
