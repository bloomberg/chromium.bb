// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_

#include <string>

#include "chrome/browser/local_discovery/gcd_base_api_flow.h"
#include "net/url_request/url_request_context_getter.h"


namespace local_discovery {

// API call flow for server-side communication with cloudprint for registration.
class PrivetConfirmApiCallFlow : public CloudPrintApiFlowDelegate {
 public:
  typedef base::Callback<void(GCDBaseApiFlow::Status /*success*/)>
      ResponseCallback;

  // Create an OAuth2-based confirmation
  PrivetConfirmApiCallFlow(net::URLRequestContextGetter* request_context,
                           OAuth2TokenService* token_service_,
                           const std::string& account_id,
                           const std::string& token,
                           const ResponseCallback& callback);

  virtual ~PrivetConfirmApiCallFlow();

  void Start();

  virtual void OnGCDAPIFlowError(GCDBaseApiFlow* flow,
                                 GCDBaseApiFlow::Status status) OVERRIDE;
  virtual void OnGCDAPIFlowComplete(GCDBaseApiFlow* flow,
                                    const base::DictionaryValue* value)
      OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() OVERRIDE;

  virtual GURL GetURL() OVERRIDE;

  GCDBaseApiFlow* GetBaseApiFlowForTests() { return &flow_; }

 private:
  GCDBaseApiFlow flow_;
  ResponseCallback callback_;
  std::string token_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
