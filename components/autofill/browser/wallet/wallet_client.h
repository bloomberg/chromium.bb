// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_H_

#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"  // For base::Closure.
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "components/autofill/browser/wallet/cart.h"
#include "components/autofill/browser/wallet/encryption_escrow_client.h"
#include "components/autofill/browser/wallet/encryption_escrow_client_observer.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/common/autocheckout_status.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {
namespace wallet {

class Address;
class FullWallet;
class Instrument;
class WalletClientDelegate;
class WalletItems;

// WalletClient is responsible for making calls to the Online Wallet backend on
// the user's behalf. The normal flow for using this class is as follows:
// 1) GetWalletItems should be called to retrieve the user's Wallet.
//   a) If the user does not have a Wallet, they must AcceptLegalDocuments and
//      SaveInstrumentAndAddress before continuing.
//   b) If the user has not accepted the most recent legal documents for
//      Wallet, they must AcceptLegalDocuments.
// 2) The user then chooses what instrument and shipping address to use for the
//    current transaction.
//   a) If they choose an instrument with a zip code only address, the billing
//      address will need to be updated using UpdateInstrument.
//   b) The user may also choose to add a new instrument or address using
//      SaveAddress, SaveInstrument, or SaveInstrumentAndAddress.
// 3) Once the user has selected the backing instrument and shipping address
//    for this transaction, a FullWallet with the fronting card is generated
//    using GetFullWallet.
//   a) GetFullWallet may return a Risk challenge for the user. In that case,
//      the user will need to verify who they are by authenticating their
//      chosen backing instrument through AuthenticateInstrument
// 4) If the user initiated Autocheckout, SendAutocheckoutStatus to notify
//    Online Wallet of the status flow to record various metrics.
//
// WalletClient is designed so only one request to Online Wallet can be outgoing
// at any one time. If |HasRequestInProgress()| is true while calling e.g.
// GetWalletItems(), the request will be queued and started later. Queued
// requests start in the order they were received.

class WalletClient
    : public net::URLFetcherDelegate,
      public EncryptionEscrowClientObserver {
 public:
  // The Risk challenges supported by users of WalletClient.
  enum RiskCapability {
    RELOGIN,
    VERIFY_CVC,
  };

  // The type of error returned by Online Wallet.
  enum ErrorType {
    // Errors to display to users.
    BUYER_ACCOUNT_ERROR,      // Risk deny, unsupported country, or account
                              // closed.
    SPENDING_LIMIT_EXCEEDED,  // User needs make a cheaper transaction or not
                              // use Online Wallet.

    // API errors.
    BAD_REQUEST,              // Request was very malformed or sent to the
                              // wrong endpoint.
    INVALID_PARAMS,           // API call had missing or invalid parameters.
    UNSUPPORTED_API_VERSION,  // The server API version of the request is no
                              // longer supported.

    // Server errors.
    INTERNAL_ERROR,           // Unknown server side error.
    SERVICE_UNAVAILABLE,      // Online Wallet is down.

    UNKNOWN_ERROR,            // Catch all error type.
  };

  struct FullWalletRequest {
   public:
    FullWalletRequest(const std::string& instrument_id,
                      const std::string& address_id,
                      const GURL& source_url,
                      const Cart& cart,
                      const std::string& google_transaction_id,
                      const std::vector<RiskCapability> risk_capabilities);
    ~FullWalletRequest();

    // The ID of the backing instrument. Should have been selected by the user
    // in some UI.
    std::string instrument_id;

    // The ID of the shipping address. Should have been selected by the user
    // in some UI.
    std::string address_id;

    // The URL that Online Wallet usage is being initiated on.
    GURL source_url;

    // Cart information.
    Cart cart;

    // The transaction ID from GetWalletItems.
    std::string google_transaction_id;

    // The Risk challenges supported by the user of WalletClient
    std::vector<RiskCapability> risk_capabilities;

   private:
    DISALLOW_ASSIGN(FullWalletRequest);
  };

  // |context_getter| is reference counted so it has no lifetime or ownership
  // requirements. |observer| must outlive |this|.
  WalletClient(net::URLRequestContextGetter* context_getter,
               WalletClientDelegate* delegate);

  virtual ~WalletClient();

  // GetWalletItems retrieves the user's online wallet. The WalletItems
  // returned may require additional action such as presenting legal documents
  // to the user to be accepted. |risk_capabilities| are the Risk challenges
  // supported by the users of WalletClient.
  void GetWalletItems(const GURL& source_url,
                      const std::vector<RiskCapability>& risk_capabilities);

  // The GetWalletItems call to the Online Wallet backend may require the user
  // to accept various legal documents before a FullWallet can be generated.
  // The |document_ids| and |google_transaction_id| are provided in the response
  // to the GetWalletItems call.
  virtual void AcceptLegalDocuments(
      const std::vector<std::string>& document_ids,
      const std::string& google_transaction_id,
      const GURL& source_url);

  // Authenticates that |card_verification_number| is for the backing instrument
  // with |instrument_id|. |obfuscated_gaia_id| is used as a key when escrowing
  // |card_verification_number|. |observer| is notified when the request is
  // complete. Used to respond to Risk challenges.
  void AuthenticateInstrument(const std::string& instrument_id,
                              const std::string& card_verification_number,
                              const std::string& obfuscated_gaia_id);

  // GetFullWallet retrieves the a FullWallet for the user.
  virtual void GetFullWallet(const FullWalletRequest& full_wallet_request);

  // SaveAddress saves a new shipping address.
  virtual void SaveAddress(const Address& address, const GURL& source_url);

  // SaveInstrument saves a new instrument.
  virtual void SaveInstrument(const Instrument& instrument,
                              const std::string& obfuscated_gaia_id,
                              const GURL& source_url);

  // SaveInstrumentAndAddress saves a new instrument and address.
  virtual void SaveInstrumentAndAddress(const Instrument& instrument,
                                        const Address& shipping_address,
                                        const std::string& obfuscated_gaia_id,
                                        const GURL& source_url);

  // SendAutocheckoutStatus is used for tracking the success of Autocheckout
  // flows. |status| is the result of the flow, |merchant_domain| is the domain
  // where the purchase occured, and |google_transaction_id| is the same as the
  // one provided by GetWalletItems.
  void SendAutocheckoutStatus(autofill::AutocheckoutStatus status,
                              const GURL& source_url,
                              const std::string& google_transaction_id);

  // UpdateAddress updates Online Wallet with the data in |address|.
  void UpdateAddress(const Address& address,
                     const GURL& source_url);

  // UpdateInstrument changes the instrument with id |instrument_id| with the
  // information in |billing_address|. Its primary use is for upgrading ZIP code
  // only addresses or those missing phone numbers. DO NOT change the name on
  // |billing_address| from the one returned by Online Wallet or this call will
  // fail.
  void UpdateInstrument(const std::string& instrument_id,
                        const Address& billing_address,
                        const GURL& source_url);

  // Whether there is a currently running request (i.e. |request_| != NULL).
  bool HasRequestInProgress() const;

  // Cancels and clears all |pending_requests_|.
  void CancelPendingRequests();

 private:
  FRIEND_TEST_ALL_PREFIXES(WalletClientTest, PendingRequest);
  FRIEND_TEST_ALL_PREFIXES(WalletClientTest, CancelPendingRequests);

  // TODO(ahutter): Implement this.
  std::string GetRiskParams() { return std::string("risky business"); }

  enum RequestType {
    NO_PENDING_REQUEST,
    ACCEPT_LEGAL_DOCUMENTS,
    AUTHENTICATE_INSTRUMENT,
    GET_FULL_WALLET,
    GET_WALLET_ITEMS,
    SAVE_ADDRESS,
    SAVE_INSTRUMENT,
    SAVE_INSTRUMENT_AND_ADDRESS,
    SEND_STATUS,
    UPDATE_ADDRESS,
    UPDATE_INSTRUMENT,
  };

  // Posts |post_body| to |url| and notifies |observer| when the request is
  // complete.
  void MakeWalletRequest(const GURL& url, const std::string& post_body);

  // Performs bookkeeping tasks for any invalid requests.
  void HandleMalformedResponse();
  void HandleNetworkError(int response_code);
  void HandleWalletError(ErrorType error_type);

  // Start the next pending request (if any).
  void StartNextPendingRequest();

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // EncryptionEscrowClientObserver:
  virtual void OnDidEncryptOneTimePad(
      const std::string& encrypted_one_time_pad,
      const std::string& session_material) OVERRIDE;
  virtual void OnDidEscrowInstrumentInformation(
      const std::string& escrow_handle)  OVERRIDE;
  virtual void OnDidEscrowCardVerificationNumber(
      const std::string& escrow_handle) OVERRIDE;
  virtual void OnDidMakeRequest() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;
  virtual void OnMalformedResponse() OVERRIDE;

  // Logs an UMA metric for each of the |required_actions|.
  void LogRequiredActions(
      const std::vector<RequiredAction>& required_actions) const;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  WalletClientDelegate* const delegate_;  // must outlive |this|.

  // The current request object.
  scoped_ptr<net::URLFetcher> request_;

  // The type of the current request. Must be NO_PENDING_REQUEST for a request
  // to be initiated as only one request may be running at a given time.
  RequestType request_type_;

  // The one time pad used for GetFullWallet encryption.
  std::vector<uint8> one_time_pad_;

  // GetFullWallet requests and requests that alter instruments rely on requests
  // made through the |encryption_escrow_client_| finishing first. The request
  // body is saved here while that those requests are in flight.
  base::DictionaryValue pending_request_body_;

  // Requests that are waiting to be run.
  std::queue<base::Closure> pending_requests_;

  // This client is repsonsible for making encryption and escrow calls to Online
  // Wallet.
  EncryptionEscrowClient encryption_escrow_client_;

  DISALLOW_COPY_AND_ASSIGN(WalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_CLIENT_H_
