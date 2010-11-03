// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
#pragma once

#include <map>
#include <string>

#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class URLRequestContextGetter;

namespace policy {

class ResponseHandler;
class URLQueryParameters;

// Device management backend implementation. This implementation makes HTTP
// requests to the policy server through the net layer.
class DeviceManagementBackendImpl : public DeviceManagementBackend,
                                    public URLFetcher::Delegate {
 public:
  explicit DeviceManagementBackendImpl(const std::string& server_url);
  virtual ~DeviceManagementBackendImpl();

  // GoogleAppsPolicyService overrides:
  virtual void ProcessRegisterRequest(
      const std::string& auth_token,
      const std::string& device_id,
      const em::DeviceRegisterRequest& request,
      DeviceRegisterResponseDelegate* response_delegate);
  virtual void ProcessUnregisterRequest(
      const std::string& device_management_token,
      const em::DeviceUnregisterRequest& request,
      DeviceUnregisterResponseDelegate* response_delegate);
  virtual void ProcessPolicyRequest(
      const std::string& device_management_token,
      const em::DevicePolicyRequest& request,
      DevicePolicyResponseDelegate* response_delegate);

  // Get the agent string (used for HTTP user agent and as agent passed to the
  // server).
  static std::string GetAgentString();

 private:
  typedef std::map<const URLFetcher*, ResponseHandler*> ResponseHandlerMap;

  // URLFetcher::Delegate override.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Create a URLFetcher for a given request message and response handler.
  void CreateFetcher(const em::DeviceManagementRequest& request,
                     ResponseHandler* handler,
                     const std::string& query_params,
                     const std::string& extra_headers);

  // Fill in the common query parameters.
  void PutCommonQueryParameters(URLQueryParameters* params);

  // Server at which to contact the service.
  const std::string server_url_;

  // The request context we use.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  // Keeps track of all in-flight requests an their response handlers.
  ResponseHandlerMap response_handlers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementBackendImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_BACKEND_IMPL_H_
