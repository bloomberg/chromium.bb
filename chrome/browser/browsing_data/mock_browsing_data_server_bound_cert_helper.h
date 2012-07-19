// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_SERVER_BOUND_CERT_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_SERVER_BOUND_CERT_HELPER_H_

#include <map>
#include <string>

#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"

// Mock for BrowsingDataServerBoundCertHelper.
class MockBrowsingDataServerBoundCertHelper
    : public BrowsingDataServerBoundCertHelper {
 public:
  explicit MockBrowsingDataServerBoundCertHelper();

  // BrowsingDataServerBoundCertHelper methods.
  virtual void StartFetching(const FetchResultCallback& callback) OVERRIDE;
  virtual void DeleteServerBoundCert(const std::string& server_id) OVERRIDE;

  // Adds a server_bound_cert sample.
  void AddServerBoundCertSample(const std::string& server_id);

  // Notifies the callback.
  void Notify();

  // Marks all server_bound_certs as existing.
  void Reset();

  // Returns true if all server_bound_certs since the last Reset() invocation
  // were deleted.
  bool AllDeleted();

 private:
  virtual ~MockBrowsingDataServerBoundCertHelper();

  FetchResultCallback callback_;

  net::ServerBoundCertStore::ServerBoundCertList server_bound_cert_list_;

  // Stores which server_bound_certs exist.
  std::map<const std::string, bool> server_bound_certs_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_SERVER_BOUND_CERT_HELPER_H_
