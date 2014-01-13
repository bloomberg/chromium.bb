// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

// Registration request is used to obtain registration IDs for applications that
// want to use GCM. It requires a set of parameters to be specified to identify
// the Chrome instance, the user, the application and a set of senders that will
// be authorized to address the application using it's assigned registration ID.
class GCM_EXPORT RegistrationRequest : public net::URLFetcherDelegate {
 public:
  // Callback completing the registration request.
  typedef base::Callback<void(const std::string& registration_id)>
      RegistrationCallback;

  // Details of the of the Registration Request. Only user's android ID and
  // its serial number are optional and can be set to 0. All other parameters
  // have to be specified to successfully complete the call.
  struct GCM_EXPORT RequestInfo {
    RequestInfo(uint64 android_id,
                uint64 security_token,
                uint64 user_android_id,
                int64 user_serial_number,
                const std::string& app_id,
                const std::string& cert,
                const std::vector<std::string>& sender_ids);
    ~RequestInfo();

    // Android ID of the device.
    uint64 android_id;
    // Security token of the device.
    uint64 security_token;
    // User's android ID. (Can be omitted in a single user scenario.)
    uint64 user_android_id;
    // User's serial number. (Can be omitted in a single user scenario.)
    int64 user_serial_number;
    // Application ID.
    std::string app_id;
    // Certificate of the application.
    std::string cert;
    // List of IDs of senders. Allowed up to 100.
    std::vector<std::string> sender_ids;
  };

  RegistrationRequest(
      const RequestInfo& request_info,
      const RegistrationCallback& callback,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  virtual ~RegistrationRequest();

  void Start();

  // URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  RegistrationCallback callback_;
  RequestInfo request_info_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_REGISTRATION_REQUEST_H_
