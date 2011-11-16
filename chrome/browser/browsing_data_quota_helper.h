// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_QUOTA_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_QUOTA_HELPER_H_
#pragma once

#include <list>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/quota/quota_types.h"

class BrowsingDataQuotaHelper;
class Profile;

struct BrowsingDataQuotaHelperDeleter {
  static void Destruct(const BrowsingDataQuotaHelper* helper);
};

// This class is an interface class to bridge between Cookies Tree and Unified
// Quota System.  This class provides a way to get usage and quota information
// through the instance.
//
// Call Create to create an instance for a profile and call StartFetching with
// a callback to fetch information asynchronously. If result is no longer needed
// after StartFetching, call CancelNotification to prevent callback.
//
// Parallel fetching is not allowed, a fetching task should start after end of
// previous task.  All method of this class should called from UI thread.
class BrowsingDataQuotaHelper
    : public base::RefCountedThreadSafe<BrowsingDataQuotaHelper,
                                        BrowsingDataQuotaHelperDeleter> {
 public:
  // QuotaInfo contains host-based quota and usage information for persistent
  // and temporary storage.
  struct QuotaInfo {
    QuotaInfo();
    explicit QuotaInfo(const std::string& host);
    QuotaInfo(const std::string& host,
              int64 temporary_usage,
              int64 persistent_usage);
    ~QuotaInfo();

    std::string host;
    int64 temporary_usage;
    int64 persistent_usage;
  };

  typedef std::list<QuotaInfo> QuotaInfoArray;
  typedef base::Callback<void(const QuotaInfoArray&)> FetchResultCallback;

  static BrowsingDataQuotaHelper* Create(Profile* profile);

  virtual void StartFetching(const FetchResultCallback& callback) = 0;
  virtual void CancelNotification() = 0;

  virtual void RevokeHostQuota(const std::string& host) = 0;

 protected:
  explicit BrowsingDataQuotaHelper(base::MessageLoopProxy* io_thread_);
  virtual ~BrowsingDataQuotaHelper();

 private:
  friend class DeleteTask<const BrowsingDataQuotaHelper>;
  friend struct BrowsingDataQuotaHelperDeleter;
  scoped_refptr<base::MessageLoopProxy> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataQuotaHelper);
};

bool operator <(const BrowsingDataQuotaHelper::QuotaInfo& lhs,
                const BrowsingDataQuotaHelper::QuotaInfo& rhs);
bool operator ==(const BrowsingDataQuotaHelper::QuotaInfo& lhs,
                 const BrowsingDataQuotaHelper::QuotaInfo& rhs);

#endif  // CHROME_BROWSER_BROWSING_DATA_QUOTA_HELPER_H_
