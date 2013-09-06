// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_ACCOUNT_MANAGER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_ACCOUNT_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/local_discovery/cloud_print_base_api_flow.h"

namespace local_discovery {

class CloudPrintAccountManager : public CloudPrintBaseApiFlow::Delegate {
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

  // BaseCloudPrintApiFlow::Delegate implementation.
  virtual void OnCloudPrintAPIFlowError(
      CloudPrintBaseApiFlow* flow,
      CloudPrintBaseApiFlow::Status status) OVERRIDE;

  virtual void OnCloudPrintAPIFlowComplete(
        CloudPrintBaseApiFlow* flow,
        const base::DictionaryValue* value) OVERRIDE;

 private:
  void ReportEmptyUserList();

  CloudPrintBaseApiFlow flow_;
  AccountsCallback callback_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_PRINT_ACCOUNT_MANAGER_H_
