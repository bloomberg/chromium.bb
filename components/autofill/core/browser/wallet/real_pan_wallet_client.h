// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WALLET_REAL_PAN_WALLET_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WALLET_REAL_PAN_WALLET_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {

class CreditCard;

namespace wallet {

// RealPanWalletClient is modelled on WalletClient. Whereas the latter is used
// for requestAutocomplete-related requests, RealPanWalletClient is used to
// import user data from Wallet for normal web Autofill.
class RealPanWalletClient : public net::URLFetcherDelegate {
 public:
  class Delegate {
   public:
    // Returns the real PAN retrieved from Wallet. |real_pan| will be empty
    // on failure.
    virtual void OnDidGetRealPan(const std::string& real_pan) = 0;

    // Called to retrieve the OAuth2 token that should be used for requests
    // to Wallet.
    virtual std::string GetOAuth2Token() = 0;
  };

  // |context_getter| is reference counted so it has no lifetime or ownership
  // requirements. |delegate| must outlive |this|. |source_url| is the url
  // of the merchant page.
  RealPanWalletClient(net::URLRequestContextGetter* context_getter,
                      Delegate* delegate);

  ~RealPanWalletClient() override;

  // The user has attempted to unmask a card with the given cvc.
  void UnmaskCard(const CreditCard& card, const std::string& cvc);

  // Cancels and clears the current |request_|.
  void CancelRequest();

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // The context for the request. Ensures the gdToken cookie is set as a header
  // in the requests to Online Wallet if it is present.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Observer class that has its various On* methods called based on the results
  // of a request to Online Wallet.
  Delegate* const delegate_;  // must outlive |this|.

  // The current request object.
  scoped_ptr<net::URLFetcher> request_;

  base::WeakPtrFactory<RealPanWalletClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RealPanWalletClient);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WALLET_REAL_PAN_WALLET_CLIENT_H_
