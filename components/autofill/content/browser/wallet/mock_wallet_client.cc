// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/mock_wallet_client.h"

namespace autofill {
namespace wallet {

MockWalletClient::MockWalletClient(net::URLRequestContextGetter* context,
                                   wallet::WalletClientDelegate* delegate,
                                   const GURL& source_url)
    : wallet::WalletClient(context, delegate, source_url) {}

MockWalletClient::~MockWalletClient() {}

void MockWalletClient::SaveToWallet(
    scoped_ptr<Instrument> instrument,
    scoped_ptr<Address> address,
    const WalletItems::MaskedInstrument* reference_instrument,
    const Address* reference_address) {
  SaveToWalletMock(instrument.get(),
                   address.get(),
                   reference_instrument,
                   reference_address);
}

}  // namespace wallet
}  // namespace autofill
