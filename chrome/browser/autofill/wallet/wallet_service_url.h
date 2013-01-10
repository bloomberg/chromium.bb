// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_SERVICE_URL_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_SERVICE_URL_H_

class GURL;

namespace wallet {

extern const char kApiKey[];
GURL GetGetWalletItemsUrl();
GURL GetGetFullWalletUrl();
GURL GetAcceptLegalDocumentsUrl();
GURL GetEncryptionUrl();
GURL GetEscrowUrl();
GURL GetSendStatusUrl();
GURL GetSaveToWalletUrl();
GURL GetPassiveAuthUrl();

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_SERVICE_URL_H_

