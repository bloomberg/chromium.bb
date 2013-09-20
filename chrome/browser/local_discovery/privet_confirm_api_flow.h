// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_

#include <string>

#include "chrome/browser/local_discovery/cloud_print_base_api_flow.h"
#include "net/url_request/url_request_context_getter.h"


namespace local_discovery {

// API call flow for server-side communication with cloudprint for registration.
class PrivetConfirmApiCallFlow : public CloudPrintBaseApiFlow::Delegate {
 public:
  typedef base::Callback<void(CloudPrintBaseApiFlow::Status /*success*/)>
      ResponseCallback;

  // Create an OAuth2-based confirmation
  PrivetConfirmApiCallFlow(net::URLRequestContextGetter* request_context,
                           OAuth2TokenService* token_service_,
                           const std::string& account_id,
                           const GURL& automated_claim_url,
                           const ResponseCallback& callback);

  // Create a cookie-based confirmation
  PrivetConfirmApiCallFlow(net::URLRequestContextGetter* request_context,
                           int  user_index,
                           const std::string& xsrf_token,
                           const GURL& automated_claim_url,
                           const ResponseCallback& callback);

  virtual ~PrivetConfirmApiCallFlow();

  void Start();

  virtual void OnCloudPrintAPIFlowError(
      CloudPrintBaseApiFlow* flow,
      CloudPrintBaseApiFlow::Status status) OVERRIDE;
  virtual void OnCloudPrintAPIFlowComplete(
      CloudPrintBaseApiFlow* flow,
      const base::DictionaryValue* value) OVERRIDE;

  CloudPrintBaseApiFlow* GetBaseApiFlowForTests() {
    return &flow_;
  }

 private:
  CloudPrintBaseApiFlow flow_;
  ResponseCallback callback_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
