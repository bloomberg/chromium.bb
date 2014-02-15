// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"

namespace autofill {
namespace wallet {

class Address;
class FullWallet;
class GaiaAccount;
class Instrument;

scoped_ptr<GaiaAccount> GetTestGaiaAccount();
std::vector<base::string16> StreetAddress(const std::string& line1,
                                          const std::string& line2);
scoped_ptr<Address> GetTestAddress();
scoped_ptr<Address> GetTestMinimalAddress();
scoped_ptr<FullWallet> GetTestFullWallet();
scoped_ptr<FullWallet> GetTestFullWalletWithRequiredActions(
    const std::vector<RequiredAction>& action);
scoped_ptr<FullWallet> GetTestFullWalletInstrumentOnly();
scoped_ptr<Instrument> GetTestInstrument();
scoped_ptr<Instrument> GetTestAddressUpgradeInstrument();
scoped_ptr<Instrument> GetTestExpirationDateChangeInstrument();
scoped_ptr<Instrument> GetTestAddressNameChangeInstrument();
scoped_ptr<WalletItems::LegalDocument> GetTestLegalDocument();
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrument();
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentExpired();
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentInvalid();
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentAmex(
    AmexPermission amex_permission);
scoped_ptr<WalletItems::MaskedInstrument> GetTestNonDefaultMaskedInstrument();
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentWithId(
    const std::string& id);
scoped_ptr<WalletItems::MaskedInstrument>
    GetTestMaskedInstrumentWithIdAndAddress(
        const std::string& id, scoped_ptr<Address> address);
scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentWithDetails(
    const std::string& id,
    scoped_ptr<Address> address,
    WalletItems::MaskedInstrument::Type type,
    WalletItems::MaskedInstrument::Status status);
scoped_ptr<Address> GetTestSaveableAddress();
scoped_ptr<Address> GetTestShippingAddress();
scoped_ptr<Address> GetTestNonDefaultShippingAddress();
scoped_ptr<WalletItems> GetTestWalletItemsWithRequiredAction(
    RequiredAction action);
scoped_ptr<WalletItems> GetTestWalletItems(AmexPermission amex_permission);
scoped_ptr<WalletItems> GetTestWalletItemsWithUsers(
    const std::vector<std::string>& users, size_t user_index);
scoped_ptr<WalletItems> GetTestWalletItemsWithDefaultIds(
    const std::string& default_instrument_id,
    const std::string& default_address_id,
    AmexPermission amex_permission);

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_
