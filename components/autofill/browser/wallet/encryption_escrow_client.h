// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_ENCRYPTION_ESCROW_CLIENT_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_ENCRYPTION_ESCROW_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
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
  // |observer| must outlive |this|.
  EncryptionEscrowClient(net::URLRequestContextGetter* context_getter,
                         EncryptionEscrowClientObserver* observer);
  virtual ~EncryptionEscrowClient();

  // Sends |one_time_pad|, a vector of cryptographically secure random bytes, to
  // Online Wallet to be encrypted. These bytes must be generated using
  // crypto/random.h.
  void EncryptOneTimePad(const std::vector<uint8>& one_time_pad);

  // Escrows the primary account number and card verfication number of
  // |new_instrument| with Online Wallet. The escrow is keyed off of
  // |obfuscated_gaia_id|.
  void EscrowInstrumentInformation(const Instrument& new_instrument,
                                   const std::string& obfuscated_gaia_id);

  // Escrows the card verfication number of an existing instrument with Online
  // Wallet. The escrow is keyed off of |obfuscated_gaia_id|.
  void EscrowCardVerificationNumber(const std::string& card_verification_number,
                                    const std::string& obfuscated_gaia_id);

  // Cancels |request_| (if it exists).
  void CancelRequest();

 protected:
  // Exposed for testing.
  const net::URLFetcher* request() const { return request_.get(); }

 private:
  enum RequestType {
    NO_PENDING_REQUEST,
    ENCRYPT_ONE_TIME_PAD,
    ESCROW_INSTRUMENT_INFORMATION,
    ESCROW_CARD_VERIFICATION_NUMBER,
  };

  // Posts |post_body| to |url|. When the request is complete, |observer_| is
  // notified of the result.
  void MakeRequest(const GURL& url, const std::string& post_body);

  // Performs bookkeeping tasks for any invalid requests.
  void HandleMalformedResponse(net::URLFetcher* request);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  EncryptionEscrowClientObserver* const observer_;

  // The current request object.
  scoped_ptr<net::URLFetcher> request_;

  // The type of the current request. Must be NO_PENDING_REQUEST for a request
  // to be initiated as only one request may be running at a given time.
  RequestType request_type_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionEscrowClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_ENCRYPTION_ESCROW_CLIENT_H_
