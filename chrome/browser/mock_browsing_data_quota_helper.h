// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_QUOTA_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_QUOTA_HELPER_H_
#pragma once

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browsing_data_quota_helper.h"

class MockBrowsingDataQuotaHelper : public BrowsingDataQuotaHelper {
 public:
  explicit MockBrowsingDataQuotaHelper(Profile* profile);

  virtual void StartFetching(FetchResultCallback* callback) OVERRIDE;
  virtual void CancelNotification() OVERRIDE;
  virtual void RevokeHostQuota(const std::string& host) OVERRIDE;

  void AddHost(const std::string& host,
               int64 temporary_usage,
               int64 persistent_usage);
  void AddQuotaSamples();
  void Notify();

 private:
  virtual ~MockBrowsingDataQuotaHelper();

  scoped_ptr<FetchResultCallback> callback_;
  std::list<QuotaInfo> response_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_QUOTA_HELPER_H_
