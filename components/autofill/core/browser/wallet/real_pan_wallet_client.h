// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WALLET_REAL_PAN_WALLET_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WALLET_REAL_PAN_WALLET_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/credit_card.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"

class IdentityProvider;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {

namespace wallet {

// RealPanWalletClient is modelled on WalletClient. Whereas the latter is used
// for requestAutocomplete-related requests, RealPanWalletClient is used to
// import user data from Wallet for normal web Autofill.
class RealPanWalletClient : public net::URLFetcherDelegate,
                            public OAuth2TokenService::Consumer  {
 public:
  class Delegate {
   public:
    // The identity provider used to get OAuth2 tokens.
    virtual IdentityProvider* GetIdentityProvider() = 0;

    // Returns the real PAN retrieved from Wallet. |real_pan| will be empty
    // on failure.
    virtual void OnDidGetRealPan(AutofillClient::GetRealPanResult result,
                                 const std::string& real_pan) = 0;
  };

  // |context_getter| is reference counted so it has no lifetime or ownership
  // requirements. |delegate| must outlive |this|. |source_url| is the url
  // of the merchant page.
  RealPanWalletClient(net::URLRequestContextGetter* context_getter,
                      Delegate* delegate);

  ~RealPanWalletClient() override;

  // Starts fetching the OAuth2 token in anticipation of future wallet requests.
  // Called as an optimization, but not strictly necessary.
  void Prepare();

  // The user has attempted to unmask a card with the given cvc.
  void UnmaskCard(const CreditCard& card,
                  const CardUnmaskDelegate::UnmaskResponse& response);

  // Cancels and clears the current |request_|.
  void CancelRequest();

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Creates |request_| from |card_| and |response_|.
  void CreateRequest();

  // Initiates a new OAuth2 token request.
  void StartTokenFetch(bool invalidate_old);

  // Adds the token to |request_| and starts the request.
  void SetOAuth2TokenAndStartRequest();

  // The context for the request.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  Delegate* const delegate_;  // must outlive |this|.

  // The card and response for the latest unmask request.
  CreditCard card_;
  CardUnmaskDelegate::UnmaskResponse response_;

  // The current Wallet request object.
  scoped_ptr<net::URLFetcher> request_;

  // The current OAuth2 token request object;
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;

  // The OAuth2 token, or empty if not fetched.
  std::string access_token_;

  // True if |this| has already retried due to a 401 response from the server.
  bool has_retried_authorization_;

  base::WeakPtrFactory<RealPanWalletClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RealPanWalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WALLET_REAL_PAN_WALLET_CLIENT_H_
