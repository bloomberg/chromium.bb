// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(estade): this file probably belongs in core/

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_SERVICE_URL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_SERVICE_URL_H_

#include <stddef.h>

class GURL;

namespace autofill {
namespace wallet {

// |user_index| is the index into the list of signed-in GAIA profiles for which
// this request is being made.
GURL GetManageInstrumentsUrl(size_t user_index);
GURL GetManageAddressesUrl(size_t user_index);

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_SERVICE_URL_H_
