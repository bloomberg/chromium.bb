// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"

namespace autofill {
namespace wallet {

class Address;
class FullWallet;

std::vector<base::string16> StreetAddress(const std::string& line1,
                                          const std::string& line2);
std::unique_ptr<Address> GetTestAddress();
std::unique_ptr<Address> GetTestMinimalAddress();
std::unique_ptr<FullWallet> GetTestFullWallet();
std::unique_ptr<FullWallet> GetTestFullWalletInstrumentOnly();
std::unique_ptr<Address> GetTestSaveableAddress();
std::unique_ptr<Address> GetTestShippingAddress();
std::unique_ptr<Address> GetTestNonDefaultShippingAddress();

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_
