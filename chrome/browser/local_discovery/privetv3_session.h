// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/local_discovery/privet_url_fetcher.h"
#include "chrome/common/extensions/api/gcd_private.h"

namespace base {
class DictionaryValue;
}

namespace local_discovery {

class PrivetHTTPClient;

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

  // Initialized session.
  void Init(const InitCallback& callback);

  void StartPairing(PairingType pairing_type, const ResultCallback& callback);

  void ConfirmCode(const std::string& code, const ResultCallback& callback);

  // Create a single /privet/v3/session/call request.
  void SendMessage(const std::string& api,
                   const base::DictionaryValue& input,
                   const MessageCallback& callback);

 private:
  // Represents request in progress using secure session.
  class Request {
   public:
    Request();
    virtual ~Request();

    virtual void OnError() = 0;
    virtual void OnParsedJson(const base::DictionaryValue& value,
                              bool has_error) = 0;

   private:
    friend class PrivetV3Session;
    scoped_ptr<FetcherDelegate> fetcher_delegate_;
  };

  void RunCallback(const base::Closure& callback);
  void DeleteFetcher(const FetcherDelegate* fetcher);

  scoped_ptr<PrivetHTTPClient> client_;
  bool code_confirmed_;
  ScopedVector<FetcherDelegate> fetchers_;
  base::WeakPtrFactory<PrivetV3Session> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PrivetV3Session);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_
