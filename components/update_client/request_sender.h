// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
#define COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace client_update_protocol {
class Ecdsa;
}

namespace net {
class URLFetcher;
}

namespace update_client {

class Configurator;

// Sends a request to one of the urls provided. The class implements a chain
// of responsibility design pattern, where the urls are tried in the order they
// are specified, until the request to one of them succeeds or all have failed.
// CUP signing is optional.
class RequestSender : public net::URLFetcherDelegate {
 public:
  // If |error| is 0, then the response is provided in the |response| parameter.
  using RequestSenderCallback =
      base::Callback<void(int error, const std::string& response)>;

  static int kErrorResponseNotTrusted;

  explicit RequestSender(const scoped_refptr<Configurator>& config);
  ~RequestSender() override;

  // |use_signing| enables CUP signing of protocol messages exchanged using
  // this class.
  void Send(bool use_signing,
            const std::string& request_body,
            const std::vector<GURL>& urls,
            const RequestSenderCallback& request_sender_callback);

 private:
  // Combines the |url| and |query_params| parameters.
  static GURL BuildUpdateUrl(const GURL& url, const std::string& query_params);

  // Decodes and returns the public key used by CUP.
  static std::string GetKey(const char* key_bytes_base64);

  // Returns the Etag of the server response or an empty string if the
  // Etag is not available.
  static std::string GetServerETag(const net::URLFetcher* source);

  // Overrides for URLFetcherDelegate.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Implements the error handling and url fallback mechanism.
  void SendInternal();

  // Called when SendInternal complets. |response_body|and |response_etag|
  // contain the body and the etag associated with the HTTP response.
  void SendInternalComplete(int error,
                            const std::string& response_body,
                            const std::string& response_etag);

  // Helper function to handle a non-continuable error in Send.
  void HandleSendError(int error);

  base::ThreadChecker thread_checker_;

  const scoped_refptr<Configurator> config_;
  bool use_signing_;
  std::vector<GURL> urls_;
  std::string request_body_;
  RequestSenderCallback request_sender_callback_;

  std::string public_key_;
  std::vector<GURL>::const_iterator cur_url_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_ptr<client_update_protocol::Ecdsa> signer_;

  DISALLOW_COPY_AND_ASSIGN(RequestSender);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_REQUEST_SENDER_H_
