// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_ACCOUNT_MANAGER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_ACCOUNT_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace local_discovery {

class CloudPrintAccountManager : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(
      const std::vector<std::string>& /*accounts*/,
      const std::string& /*xsrf_token*/)> AccountsCallback;

  CloudPrintAccountManager(net::URLRequestContextGetter* request_context,
                           const std::string& cloud_print_url,
                           int token_user_index,
                           const AccountsCallback& callback);
  virtual ~CloudPrintAccountManager();

  void Start();

  // URLFetcher implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  void ReportEmptyUserList();

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  std::string cloud_print_url_;
  int token_user_index_;
  AccountsCallback callback_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_ACCOUNT_MANAGER_H_
