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
  typedef base::Callback<void(const std::string& ticket_id)> ResponseCallback;

  explicit GCDRegistrationTicketRequest(const ResponseCallback& callback);
  virtual ~GCDRegistrationTicketRequest();

  // GCDApiFlowImpl::Request implementation.
  virtual void GetUploadData(std::string* upload_type,
                             std::string* upload_data) OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() OVERRIDE;
  virtual void OnGCDAPIFlowError(GCDApiFlow::Status status) OVERRIDE;
  virtual void OnGCDAPIFlowComplete(
      const base::DictionaryValue& value) OVERRIDE;
  virtual GURL GetURL() OVERRIDE;

 private:
  ResponseCallback callback_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_GCD_REGISTRATION_TICKET_REQUEST_H_
