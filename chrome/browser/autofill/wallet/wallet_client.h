// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/encryption_escrow_client.h"
#include "chrome/browser/autofill/wallet/encryption_escrow_client_observer.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/common/autofill/autocheckout_status.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {
namespace wallet {

class Address;
class Cart;
class FullWallet;
class Instrument;
class WalletClientObserver;
class WalletItems;

// WalletClient is responsible for making calls to the Online Wallet backend on
// the user's behalf. The normal flow for using this class is as follows:
// 1) GetWalletItems should be called to retrieve the user's Wallet.
//   a) If the user does not have a Wallet, they must AcceptLegalDocuments and
//      SaveInstrumentAndAddress before continuing.
//   b) If the user has not acccepte the most recent legal documents for
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
// WalletClient is designed so only one request to Online Wallet can be
// outgoing at any one time. Implementors of WalletClientObserver should wait
// for a callback to be called for an outgoing request before making another.
class WalletClient
    : public net::URLFetcherDelegate,
      public EncryptionEscrowClientObserver {
 public:
  // |context_getter| is reference counted so it has no lifetime or ownership
  // requirements.
  explicit WalletClient(net::URLRequestContextGetter* context_getter);

  virtual ~WalletClient();

  // GetWalletItems retrieves the user's online wallet. The WalletItems
  // returned may require additional action such as presenting legal documents
  // to the user to be accepted.
  void GetWalletItems(base::WeakPtr<WalletClientObserver> observer);

  // The GetWalletItems call to the Online Wallet backend may require the user
  // to accept various legal documents before a FullWallet can be generated.
  // The |document_ids| and |google_transaction_id| are provided in the response
  // to the GetWalletItems call.
  void AcceptLegalDocuments(const std::vector<std::string>& document_ids,
                            const std::string& google_transaction_id,
                            base::WeakPtr<WalletClientObserver> observer);

  // Authenticates that |card_verification_number| is for the backing instrument
  // with |instrument_id|. |obfuscated_gaia_id| is used as a key when escrowing
  // |card_verification_number|. |observer| is notified when the request is
  // complete. Used to respond to Risk challenges.
  void AuthenticateInstrument(const std::string& instrument_id,
                              const std::string& card_verification_number,
                              const std::string& obfuscated_gaia_id,
                              base::WeakPtr<WalletClientObserver> observer);

  // GetFullWallet retrieves the a FullWallet for the user. |instrument_id| and
  // |adddress_id| should have been selected by the user in some UI,
  // |merchant_domain| should come from the BrowserContext, the |cart|
  // information will have been provided by the browser, and
  // |google_transaction_id| is the same one that GetWalletItems returns.
  void GetFullWallet(const std::string& instrument_id,
                     const std::string& address_id,
                     const std::string& merchant_domain,
                     const Cart& cart,
                     const std::string& google_transaction_id,
                     base::WeakPtr<WalletClientObserver> observer);

  // SaveAddress saves a new shipping address.
  void SaveAddress(const Address& address,
                   base::WeakPtr<WalletClientObserver> observer);

  // SaveInstrument saves a new instrument.
  void SaveInstrument(const Instrument& instrument,
                      const std::string& obfuscated_gaia_id,
                      base::WeakPtr<WalletClientObserver> observer);

  // SaveInstrumentAndAddress saves a new instrument and address.
  void SaveInstrumentAndAddress(const Instrument& instrument,
                                const Address& shipping_address,
                                const std::string& obfuscated_gaia_id,
                                base::WeakPtr<WalletClientObserver> observer);

  // SendAutocheckoutStatus is used for tracking the success of Autocheckout
  // flows. |status| is the result of the flow, |merchant_domain| is the domain
  // where the purchase occured, and |google_transaction_id| is the same as the
  // one provided by GetWalletItems.
  void SendAutocheckoutStatus(autofill::AutocheckoutStatus status,
                              const std::string& merchant_domain,
                              const std::string& google_transaction_id,
                              base::WeakPtr<WalletClientObserver> observer);

  // UpdateInstrument changes the instrument with id |instrument_id| with the
  // information in |billing_address|. Its primary use is for upgrading ZIP code
  // only addresses or those missing phone numbers. DO NOT change the name on
  // |billing_address| from the one returned by Online Wallet or this call will
  // fail.
  void UpdateInstrument(const std::string& instrument_id,
                        const Address& billing_address,
                        base::WeakPtr<WalletClientObserver> observer);

  // Whether there is a currently running request (i.e. |request_| != NULL).
  bool HasRequestInProgress() const;

 private:
  // TODO(ahutter): Implement this.
  std::string GetRiskParams() { return ""; }

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
    UPDATE_INSTRUMENT,
  };

  // Posts |post_body| to |url| and notifies |observer| when the request is
  // complete.
  void MakeWalletRequest(const GURL& url,
                         const std::string& post_body,
                         base::WeakPtr<WalletClientObserver> observer);

  // Performs bookkeeping tasks for any invalid requests.
  void HandleMalformedResponse(net::URLFetcher* request);

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
  virtual void OnNetworkError(int response_code) OVERRIDE;
  virtual void OnMalformedResponse() OVERRIDE;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  base::WeakPtr<WalletClientObserver> observer_;

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

  // This client is repsonsible for making encryption and escrow calls to Online
  // Wallet.
  EncryptionEscrowClient encryption_escrow_client_;

  base::WeakPtrFactory<WalletClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_H_
