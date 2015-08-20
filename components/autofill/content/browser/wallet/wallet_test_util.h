// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

namespace autofill {
namespace wallet {

class Address;
class FullWallet;

std::vector<base::string16> StreetAddress(const std::string& line1,
                                          const std::string& line2);
scoped_ptr<Address> GetTestAddress();
scoped_ptr<Address> GetTestMinimalAddress();
scoped_ptr<FullWallet> GetTestFullWallet();
scoped_ptr<FullWallet> GetTestFullWalletInstrumentOnly();
scoped_ptr<Address> GetTestSaveableAddress();
scoped_ptr<Address> GetTestShippingAddress();
scoped_ptr<Address> GetTestNonDefaultShippingAddress();

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_WALLET_TEST_UTIL_H_
