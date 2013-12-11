// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_SERVICE_URL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_SERVICE_URL_H_

#include <stddef.h>

class GURL;

namespace autofill {
namespace wallet {

// |user_index| is the index into the list of signed-in GAIA profiles for which
// this request is being made.
GURL GetGetWalletItemsUrl(size_t user_index);
GURL GetGetFullWalletUrl(size_t user_index);
GURL GetManageInstrumentsUrl(size_t user_index);
GURL GetManageAddressesUrl(size_t user_index);
GURL GetAcceptLegalDocumentsUrl(size_t user_index);
GURL GetAuthenticateInstrumentUrl(size_t user_index);
GURL GetSendStatusUrl(size_t user_index);
GURL GetSaveToWalletNoEscrowUrl(size_t user_index);
GURL GetSaveToWalletUrl(size_t user_index);
GURL GetPassiveAuthUrl(size_t user_index);

// URL to visit for presenting the user with a sign-in dialog.
GURL GetSignInUrl();

// The the URL to use as a continue parameter in the sign-in URL.
// A redirect to this URL will occur once sign-in is complete.
GURL GetSignInContinueUrl();

// Returns true if |url| is an acceptable variant of the sign-in continue
// url.  Can be used for detection of navigation to the continue url.  If
// |url| is a sign-in url, |user_index| will also be filled in to indicate
// which user account just signed in.
bool IsSignInContinueUrl(const GURL& url, size_t* user_index);

// Returns whether |url| is related to sign-in. This is used to determine
// whether to open a new link from the Autofill dialog's sign-in webview.
bool IsSignInRelatedUrl(const GURL& url);

// Whether calls to Online Wallet are hitting the production server rather than
// a sandbox or some malicious endpoint.
bool IsUsingProd();

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_SERVICE_URL_H_
