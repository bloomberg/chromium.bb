// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "components/autofill/browser/wallet/wallet_items.h"

namespace autofill {
namespace wallet {

class Instrument;
class Address;

scoped_ptr<Address> GetTestAddress();
scoped_ptr<Instrument> GetTestInstrument();
scoped_ptr<WalletItems::LegalDocument> GetTestLegalDocument();
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrument();
scoped_ptr<Address> GetTestSaveableAddress();
scoped_ptr<Address> GetTestShippingAddress();
scoped_ptr<WalletItems> GetTestWalletItems();

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_WALLET_WALLET_TEST_UTIL_H_
