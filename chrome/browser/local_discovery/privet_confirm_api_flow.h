// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "net/url_request/url_request_context_getter.h"

namespace local_discovery {

// API call flow for server-side communication with CloudPrint for registration.
class PrivetConfirmApiCallFlow : public CloudPrintApiFlowRequest {
 public:
  typedef base::Callback<void(GCDApiFlow::Status /*success*/)> ResponseCallback;

  // Create an OAuth2-based confirmation
  PrivetConfirmApiCallFlow(const std::string& token,
                           const ResponseCallback& callback);

  virtual ~PrivetConfirmApiCallFlow();

  virtual void OnGCDAPIFlowError(GCDApiFlow::Status status) OVERRIDE;
  virtual void OnGCDAPIFlowComplete(
      const base::DictionaryValue& value) OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() OVERRIDE;

  virtual GURL GetURL() OVERRIDE;

 private:
  ResponseCallback callback_;
  std::string token_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONFIRM_API_FLOW_H_
