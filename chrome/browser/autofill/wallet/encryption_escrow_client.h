// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_ENCRYPTION_ESCROW_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_ENCRYPTION_ESCROW_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {
namespace wallet {

class EncryptionEscrowClientObserver;
class Instrument;

// EncrytionEscrowClient is responsible for making calls to the Online Wallet
// encryption and escrow backend.
class EncryptionEscrowClient : public net::URLFetcherDelegate {
 public:
  explicit EncryptionEscrowClient(net::URLRequestContextGetter* context_getter);
  virtual ~EncryptionEscrowClient();

  // Sends |one_time_pad|, a vector of cryptographically secure random bytes, to
  // Online Wallet to be encrypted. These bytes must be generated using
  // crypto/random.h. |observer| is notified when the request is complete.
  void EncryptOneTimePad(
      const std::vector<uint8>& one_time_pad,
      base::WeakPtr<EncryptionEscrowClientObserver> observer);

  // Escrows the primary account number and card verfication number of
  // |new_instrument| with Online Wallet. The escrow is keyed off of
  // |obfuscated_gaia_id|. |observer| is notified when the request is complete.
  void EscrowSensitiveInformation(
      const Instrument& new_instrument,
      const std::string& obfuscated_gaia_id,
      base::WeakPtr<EncryptionEscrowClientObserver> observer);

 private:
  enum RequestType {
    NO_PENDING_REQUEST,
    ENCRYPT_ONE_TIME_PAD,
    ESCROW_SENSITIVE_INFORMATION,
  };

  // Posts |post_body| to |url|. When the request is complete, the |observer|
  // is notified of the result.
  void MakeRequest(
      const GURL& url,
      const std::string& post_body,
      base::WeakPtr<EncryptionEscrowClientObserver> observer);

  // Performs bookkeeping tasks for any invalid requests.
  void HandleMalformedResponse(net::URLFetcher* request);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  base::WeakPtr<EncryptionEscrowClientObserver> observer_;

  // The current request object.
  scoped_ptr<net::URLFetcher> request_;

  // The type of the current request. Must be NO_PENDING_REQUEST for a request
  // to be initiated as only one request may be running at a given time.
  RequestType request_type_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionEscrowClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_ENCRYPTION_ESCROW_CLIENT_H_
