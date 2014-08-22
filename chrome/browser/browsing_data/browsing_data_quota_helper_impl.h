// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "webkit/common/quota/quota_types.h"

class GURL;

namespace storage {
class QuotaManager;
}

// Implementation of BrowsingDataQuotaHelper.  Since a client of
// BrowsingDataQuotaHelper should live in UI thread and QuotaManager lives in
// IO thread, we have to communicate over thread using PostTask.
class BrowsingDataQuotaHelperImpl : public BrowsingDataQuotaHelper {
 public:
  virtual void StartFetching(const FetchResultCallback& callback) OVERRIDE;
  virtual void RevokeHostQuota(const std::string& host) OVERRIDE;

 private:
  BrowsingDataQuotaHelperImpl(base::MessageLoopProxy* ui_thread,
                              base::MessageLoopProxy* io_thread,
                              storage::QuotaManager* quota_manager);
  virtual ~BrowsingDataQuotaHelperImpl();

  void FetchQuotaInfo();

  // Callback function for GetOriginModifiedSince.
  void GotOrigins(const std::set<GURL>& origins, storage::StorageType type);

  void ProcessPendingHosts();
  void GetHostUsage(const std::string& host, storage::StorageType type);

  // Callback function for GetHostUsage.
  void GotHostUsage(const std::string& host,
                    storage::StorageType type,
                    int64 usage);

  void OnComplete();
  void DidRevokeHostQuota(storage::QuotaStatusCode status, int64 quota);

  scoped_refptr<storage::QuotaManager> quota_manager_;
  FetchResultCallback callback_;

  typedef std::set<std::pair<std::string, storage::StorageType> > PendingHosts;
  PendingHosts pending_hosts_;
  std::map<std::string, QuotaInfo> quota_info_;

  bool is_fetching_;

  scoped_refptr<base::MessageLoopProxy> ui_thread_;
  scoped_refptr<base::MessageLoopProxy> io_thread_;
  base::WeakPtrFactory<BrowsingDataQuotaHelperImpl> weak_factory_;

  friend class BrowsingDataQuotaHelper;
  friend class BrowsingDataQuotaHelperTest;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataQuotaHelperImpl);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
