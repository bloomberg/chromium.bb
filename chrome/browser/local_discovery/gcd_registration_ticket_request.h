// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_GCD_REGISTRATION_TICKET_REQUEST_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_GCD_REGISTRATION_TICKET_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"

namespace local_discovery {

class GCDRegistrationTicketRequest : public GCDApiFlowRequest {
 public:
  // |ticket_id| contains the registration ticket ID, or an empty string in case
  // of an error.
  typedef base::Callback<void(const std::string& ticket_id,
                              const std::string& device_id)> ResponseCallback;

  explicit GCDRegistrationTicketRequest(const ResponseCallback& callback);
  ~GCDRegistrationTicketRequest() override;

  // GCDApiFlowImpl::Request implementation.
  void GetUploadData(std::string* upload_type,
                     std::string* upload_data) override;
  net::URLFetcher::RequestType GetRequestType() override;
  void OnGCDAPIFlowError(GCDApiFlow::Status status) override;
  void OnGCDAPIFlowComplete(const base::DictionaryValue& value) override;
  GURL GetURL() override;

 private:
  ResponseCallback callback_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_GCD_REGISTRATION_TICKET_REQUEST_H_
