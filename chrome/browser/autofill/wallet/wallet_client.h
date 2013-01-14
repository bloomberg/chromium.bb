// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/common/autofill/autocheckout_status.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace net {
class URLRequestContextGetter;
class URLFetcher;
}

namespace wallet {

class Address;
class Cart;
class FullWallet;
class WalletItems;

// WalletClient is responsible for making calls to the Online Wallet backend on
// the user's behalf.
class WalletClient : public net::URLFetcherDelegate {
 public:
  // WalletClientObserver is to be implemented any classes making calls with
  // WalletClient. The appropriate callback method will be called on
  // WalletClientObserver with the response from the Online Wallet backend.
  class WalletClientObserver {
   public:
    // Called when an AcceptLegalDocuments request finishes successfully.
    virtual void OnAcceptLegalDocuments() = 0;

    // Called when an EncryptOtp request finishes successfully.
    virtual void OnEncryptOtp(const std::string& encrypted_otp,
                              const std::string& session_material) = 0;

    // Called when an EscrowSensitiveInformation request finishes successfully.
    // |escrow_handle| must be used when saving a new instrument using
    // SaveInstrument or SaveAdressAndInstrument.
    virtual void OnDidEscrowSensitiveInformation(
        const std::string& escrow_handle) = 0;

    // Called when a GetFullWallet request finishes successfully. Caller owns
    // the input pointer.
    virtual void OnGetFullWallet(FullWallet* full_wallet) = 0;

    // Called when a GetWalletItems request finishes successfully. Caller owns
    // the input pointer.
    virtual void OnGetWalletItems(WalletItems* wallet_items) = 0;

    // Called when a SendAutocheckoutStatus request finishes successfully.
    virtual void OnSendAutocheckoutStatus() = 0;

    // TODO(ahutter): This is going to need more arguments, probably an error
    // code and a message for the user.
    // Called when a request fails due to an Online Wallet error.
    virtual void OnWalletError() = 0;

    // Called when a request fails due to a network error or if the response was
    // invalid.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~WalletClientObserver() {}
  };
  explicit WalletClient(net::URLRequestContextGetter* context_getter);
  virtual ~WalletClient();

  // GetWalletItems retrieves the user's online wallet. The WalletItems
  // returned may require additional action such as presenting legal documents
  // to the user to be accepted.
  void GetWalletItems(WalletClientObserver* observer);

  // The GetWalletItems call to the Online Wallet backend may require the user
  // to accept various legal documents before a FullWallet can be generated.
  // The |document_ids| and |google_transaction_id| are provided in the response
  // to the GetWalletItems call.
  void AcceptLegalDocuments(const std::vector<std::string>& document_ids,
                            const std::string& google_transaction_id,
                            WalletClientObserver* observer);

  // Before calling GetFullWallet, the client must encrypt a one time pad,
  // |otp|, of crytographically secure random bytes. These bytes must be
  // generated using crypto/random.h.
  void EncryptOtp(const void* otp,
                  size_t length,
                  WalletClientObserver* observer);

  // Before calling SaveInstrument or SaveAddressAndInstrument, the client must
  // escrow |primary_account_number| and |card_verfication_number| with Google
  // Wallet.
  void EscrowSensitiveInformation(const std::string& primary_account_number,
                                  const std::string& card_verification_number,
                                  const std::string& obfuscated_gaia_id,
                                  WalletClientObserver* observer);

  // GetFullWallet retrieves the a FullWallet for the user. |instrument_id| and
  // |adddress_id| should have been selected by the user in some UI,
  // |merchant_domain| should come from the BrowserContext, the |cart|
  // information will have been provided by the browser, |google_transaction_id|
  // is the same one that GetWalletItems returns, and |encrypted_otp| and
  // |session_material| are the results of the EncryptOtp call.
  void GetFullWallet(const std::string& instrument_id,
                     const std::string& address_id,
                     const std::string& merchant_domain,
                     const Cart& cart,
                     const std::string& google_transaction_id,
                     const std::string& encrypted_otp,
                     const std::string& session_material,
                     WalletClientObserver* observer);

  // SendAutocheckoutStatus is used for tracking the success of Autocheckout
  // flows. |status| is the result of the flow, |merchant_domain| is the domain
  // where the purchase occured, and |google_transaction_id| is the same as the
  // one provided by GetWalletItems.
  void SendAutocheckoutStatus(autofill::AutocheckoutStatus status,
                              const std::string& merchant_domain,
                              const std::string& google_transaction_id,
                              WalletClientObserver* observer);

 private:
  // TODO(ahutter): Implement this.
  std::string GetRiskParams() { return ""; }

  enum RequestType {
    NO_PENDING_REQUEST,
    ACCEPT_LEGAL_DOCUMENTS,
    ENCRYPT_OTP,
    ESCROW_SENSITIVE_INFORMATION,
    GET_FULL_WALLET,
    GET_WALLET_ITEMS,
    SEND_STATUS,
  };

  void MakeWalletRequest(const GURL& url,
                         const std::string& post_body,
                         WalletClient::WalletClientObserver* observer,
                         const std::string& content_type);
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  WalletClientObserver* observer_;
  // The current request object.
  scoped_ptr<net::URLFetcher> request_;
  // The type of the current request. Must be NO_PENDING_REQUEST for a request
  // to be initiated as only one request may be running at a given time.
  RequestType request_type_;
  DISALLOW_COPY_AND_ASSIGN(WalletClient);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_CLIENT_H_

