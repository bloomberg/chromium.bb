// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_SERVICE_URL_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_SERVICE_URL_H_

class GURL;

namespace autofill {
namespace wallet {

GURL GetGetWalletItemsUrl();
GURL GetGetFullWalletUrl();
GURL GetAcceptLegalDocumentsUrl();
GURL GetEncryptionUrl();
GURL GetEscrowUrl();
GURL GetSendStatusUrl();
GURL GetSaveToWalletUrl();
GURL GetPassiveAuthUrl();

// URL to visit for presenting the user with a sign-in dialog.
GURL GetSignInUrl();

// The the URL to use as a continue parameter in the sign-in URL.
// A redirect to this URL will occur once sign-in is complete.
GURL GetSignInContinueUrl();

// Returns true if |url| is an acceptable variant of the sign-in continue
// url.  Can be used for detection of navigation to the continue url.
bool IsSignInContinueUrl(const GURL& url);

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_SERVICE_URL_H_
