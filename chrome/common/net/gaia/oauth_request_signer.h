// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_GAIA_OAUTH_REQUEST_SIGNER_H_
#define CHROME_COMMON_NET_GAIA_OAUTH_REQUEST_SIGNER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"

class GURL;

// Implements the OAuth request signing process as described here:
//   http://oauth.net/core/1.0/#signing_process
//
// NOTE: Currently the only supported SignatureMethod is HMAC_SHA1_SIGNATURE
class OAuthRequestSigner {
 public:
  enum SignatureMethod {
    HMAC_SHA1_SIGNATURE,
    RSA_SHA1_SIGNATURE,
    PLAINTEXT_SIGNATURE
  };

  enum HttpMethod {
    GET_METHOD,
    POST_METHOD
  };

  typedef std::map<std::string,std::string> Parameters;

  // Signs a request specified as URL string, complete with parameters.
  //
  // If HttpMethod is GET_METHOD, the signed result is the full URL, otherwise
  // it is the request parameters, including the oauth_signature field.
  static bool ParseAndSign(const GURL& request_url_with_parameters,
                           SignatureMethod signature_method,
                           HttpMethod http_method,
                           const std::string& consumer_key,
                           const std::string& consumer_secret,
                           const std::string& token_key,
                           const std::string& token_secret,
                           std::string* signed_result);

  // Signs a request specified as the combination of a base URL string, with
  // parameters included in a separate map data structure.  NOTE: The base URL
  // string must not contain a question mark (?) character.  If it does,
  // you can use ParseAndSign() instead.
  //
  // If HttpMethod is GET_METHOD, the signed result is the full URL, otherwise
  // it is the request parameters, including the oauth_signature field.
  static bool Sign(const GURL& request_base_url,
                   const Parameters& parameters,
                   SignatureMethod signature_method,
                   HttpMethod http_method,
                   const std::string& consumer_key,
                   const std::string& consumer_secret,
                   const std::string& token_key,
                   const std::string& token_secret,
                   std::string* signed_result);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OAuthRequestSigner);
};

#endif  // CHROME_COMMON_NET_GAIA_OAUTH_REQUEST_SIGNER_H_
