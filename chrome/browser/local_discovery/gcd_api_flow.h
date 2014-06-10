// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_GCD_API_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_GCD_API_FLOW_H_

#include <string>

#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace local_discovery {

// API flow for communicating with cloud print and cloud devices.
class GCDApiFlow {
 public:
  // TODO(noamsml): Better error model for this class.
  enum Status {
    SUCCESS,
    ERROR_TOKEN,
    ERROR_NETWORK,
    ERROR_HTTP_CODE,
    ERROR_FROM_SERVER,
    ERROR_MALFORMED_RESPONSE
  };

  // Provides GCDApiFlowImpl with parameters required to make request.
  // Parses results of requests.
  class Request {
   public:
    Request();
    virtual ~Request();

    virtual void OnGCDAPIFlowError(Status status) = 0;

    virtual void OnGCDAPIFlowComplete(const base::DictionaryValue& value) = 0;

    virtual GURL GetURL() = 0;

    virtual std::string GetOAuthScope() = 0;

    virtual net::URLFetcher::RequestType GetRequestType();

    virtual std::vector<std::string> GetExtraRequestHeaders() = 0;

    // If there is no data, set upload_type and upload_data to ""
    virtual void GetUploadData(std::string* upload_type,
                               std::string* upload_data);

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  GCDApiFlow();
  virtual ~GCDApiFlow();

  static scoped_ptr<GCDApiFlow> Create(
      net::URLRequestContextGetter* request_context,
      OAuth2TokenService* token_service,
      const std::string& account_id);

  virtual void Start(scoped_ptr<Request> request) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCDApiFlow);
};

class GCDApiFlowRequest : public GCDApiFlow::Request {
 public:
  GCDApiFlowRequest();
  virtual ~GCDApiFlowRequest();

  // GCDApiFlowRequest implementation
  virtual std::string GetOAuthScope() OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCDApiFlowRequest);
};

class CloudPrintApiFlowRequest : public GCDApiFlow::Request {
 public:
  CloudPrintApiFlowRequest();
  virtual ~CloudPrintApiFlowRequest();

  // GCDApiFlowRequest implementation
  virtual std::string GetOAuthScope() OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintApiFlowRequest);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_GCD_API_FLOW_H_
