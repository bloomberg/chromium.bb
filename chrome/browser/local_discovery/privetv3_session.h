// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "net/url_request/url_fetcher.h"

namespace base {
class DictionaryValue;
}

namespace crypto {
class P224EncryptedKeyExchange;
}

namespace local_discovery {

class PrivetHTTPClient;
class PrivetJSONOperation;
class PrivetURLFetcher;

// Manages secure communication between browser and local Privet device.
class PrivetV3Session {
 private:
  class FetcherDelegate;

 public:
  typedef extensions::api::gcd_private::PairingType PairingType;
  typedef extensions::api::gcd_private::Status Result;

  typedef base::Callback<
      void(Result result, const std::vector<PairingType>& types)> InitCallback;

  typedef base::Callback<void(Result result)> ResultCallback;
  typedef base::Callback<void(Result result,
                              const base::DictionaryValue& response)>
      MessageCallback;

  explicit PrivetV3Session(scoped_ptr<PrivetHTTPClient> client);
  ~PrivetV3Session();

  // Initializes session. Queries /privet/info and returns supported pairing
  // types in callback.
  void Init(const InitCallback& callback);

  // Starts pairing by calling /privet/v3/pairing/start.
  void StartPairing(PairingType pairing_type, const ResultCallback& callback);

  // Confirms pairing code by calling /privet/v3/pairing/confirm.
  // Calls /privet/v3/pairing/auth after pairing.
  void ConfirmCode(const std::string& code, const ResultCallback& callback);

  // TODO(vitalybuka): Make HTTPS request to device with certificate validation.
  void SendMessage(const std::string& api,
                   const base::DictionaryValue& input,
                   const MessageCallback& callback);

 private:
  friend class PrivetV3SessionTest;
  FRIEND_TEST_ALL_PREFIXES(PrivetV3SessionTest, Pairing);

  void OnInfoDone(const InitCallback& callback,
                  Result result,
                  const base::DictionaryValue& response);
  void OnPairingStartDone(const ResultCallback& callback,
                          Result result,
                          const base::DictionaryValue& response);
  void OnPairingConfirmDone(const ResultCallback& callback,
                            Result result,
                            const base::DictionaryValue& response);
  void OnAuthenticateDone(const ResultCallback& callback,
                          Result result,
                          const base::DictionaryValue& response);

  void RunCallback(const base::Closure& callback);
  void StartGetRequest(const std::string& api, const MessageCallback& callback);
  void StartPostRequest(const std::string& api,
                        const base::DictionaryValue& input,
                        const MessageCallback& callback);
  PrivetURLFetcher* CreateFetcher(const std::string& api,
                                  net::URLFetcher::RequestType request_type,
                                  const MessageCallback& callback);
  void DeleteFetcher(const FetcherDelegate* fetcher);
  void Cancel();

  // Creates instances of PrivetURLFetcher.
  scoped_ptr<PrivetHTTPClient> client_;

  // Current authentication token.
  std::string privet_auth_token_;

  // ID of the session received from pairing/start.
  std::string session_id_;

  // Device commitment received from pairing/start.
  std::string commitment_;

  // Key exchange algorithm for pairing.
  scoped_ptr<crypto::P224EncryptedKeyExchange> spake_;

  // HTTPS certificate fingerprint received during pairing.
  std::string fingerprint_;

  // List of fetches to cancel when session is destroyed.
  ScopedVector<FetcherDelegate> fetchers_;

  // Intercepts POST requests. Used by tests only.
  base::Callback<void(const base::DictionaryValue&)> on_post_data_;

  base::WeakPtrFactory<PrivetV3Session> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PrivetV3Session);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_
